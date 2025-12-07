#include "gpagent/ui/chat_backend.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/core/uuid.hpp"
#include "gpagent/tools/tool_executor.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QVariantMap>

namespace gpagent::ui {

// ChatWorker implementation
ChatWorker::ChatWorker(agent::Orchestrator* orchestrator)
    : m_orchestrator(orchestrator)
{
}

void ChatWorker::processMessage(const QString& message)
{
    if (!m_orchestrator) {
        emit error("Orchestrator not initialized");
        return;
    }

    auto streamCallback = [this](const std::string& chunk) {
        emit streamingChunk(QString::fromStdString(chunk));
    };

    auto eventCallback = [this](const agent::AgentEventData& event) {
        emit agentEvent(static_cast<int>(event.event),
                       QString::fromStdString(event.message));
    };

    auto result = m_orchestrator->process_with_events(
        message.toStdString(),
        eventCallback,
        streamCallback
    );

    if (result.is_ok()) {
        emit responseComplete(QString::fromStdString(result.value()));
    } else {
        emit error(QString::fromStdString(result.error().message));
    }
}

// ChatBackend implementation
ChatBackend::ChatBackend(QObject* parent)
    : QObject(parent)
    , m_messages(new MessageModel(this))
{
}

ChatBackend::~ChatBackend()
{
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

bool ChatBackend::initialize(const QString& configPath)
{
    try {
        // Load or create default config
        QString path = configPath;
        if (path.isEmpty()) {
            QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
            QDir().mkpath(configDir + "/gpagent");
            path = configDir + "/gpagent/config.yaml";
        }

        m_config = std::make_unique<core::Config>(
            core::Config::load_or_default(path.toStdString())
        );
        m_config->expand_paths();

        // Initialize LLM Gateway
        m_llmGateway = std::make_unique<llm::LLMGateway>(
            m_config->llm, m_config->api_keys
        );
        auto llmResult = m_llmGateway->initialize();
        if (!llmResult.is_ok()) {
            emit errorOccurred(QString::fromStdString(llmResult.error().message));
            return false;
        }

        // Initialize Tool Registry
        m_toolRegistry = std::make_unique<tools::ToolRegistry>(m_config->tools);
        m_toolRegistry->register_builtins();

        // Initialize Tool Executor
        m_toolExecutor = std::make_unique<tools::ToolExecutor>(
            *m_toolRegistry, m_config->concurrency
        );

        // Initialize Memory Manager
        m_memoryManager = std::make_unique<memory::MemoryManager>(m_config->memory);
        auto memResult = m_memoryManager->initialize();
        if (!memResult.is_ok()) {
            emit errorOccurred(QString::fromStdString(memResult.error().message));
            return false;
        }

        // Initialize Context Manager
        m_contextManager = std::make_unique<context::ContextManager>(
            m_config->context, *m_llmGateway
        );

        // Create Orchestrator
        agent::Orchestrator::Config orchConfig;
        orchConfig.system_prompt = "You are a helpful AI assistant.";
        orchConfig.max_turns_per_task = 50;

        m_orchestrator = std::make_unique<agent::Orchestrator>(
            orchConfig,
            *m_llmGateway,
            *m_toolRegistry,
            *m_toolExecutor,
            *m_memoryManager,
            *m_contextManager
        );

        // Pass app config to orchestrator for tool access
        m_orchestrator->set_app_config(m_config.get());

        auto orchResult = m_orchestrator->initialize();
        if (!orchResult.is_ok()) {
            emit errorOccurred(QString::fromStdString(orchResult.error().message));
            return false;
        }

        // Setup worker thread
        setupWorker();

        m_currentModel = QString::fromStdString(m_config->llm.primary_model);
        emit currentModelChanged();
        emit initialized();

        return true;

    } catch (const std::exception& e) {
        emit errorOccurred(QString("Initialization failed: %1").arg(e.what()));
        return false;
    }
}

void ChatBackend::setupWorker()
{
    m_workerThread = new QThread(this);
    m_worker = new ChatWorker(m_orchestrator.get());
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &ChatBackend::destroyed, m_workerThread, &QThread::quit);

    connect(m_worker, &ChatWorker::streamingChunk,
            this, &ChatBackend::onStreamingChunk, Qt::QueuedConnection);
    connect(m_worker, &ChatWorker::responseComplete,
            this, &ChatBackend::onResponseComplete, Qt::QueuedConnection);
    connect(m_worker, &ChatWorker::error,
            this, &ChatBackend::onError, Qt::QueuedConnection);
    connect(m_worker, &ChatWorker::agentEvent,
            this, &ChatBackend::onAgentEvent, Qt::QueuedConnection);

    m_workerThread->start();
}

void ChatBackend::sendMessage(const QString& content)
{
    if (content.trimmed().isEmpty() || m_isBusy) {
        return;
    }

    // Add user message to model
    m_messages->addUserMessage(content);

    // Start streaming response
    m_messages->beginStreaming();

    setBusy(true);
    setStatusMessage("Thinking...");

    // Process in worker thread
    QMetaObject::invokeMethod(m_worker, "processMessage",
                              Qt::QueuedConnection,
                              Q_ARG(QString, content));
}

void ChatBackend::stopGeneration()
{
    if (m_orchestrator) {
        m_orchestrator->abort_task();
    }
}

void ChatBackend::clearConversation()
{
    m_messages->clear();
    if (m_memoryManager) {
        m_memoryManager->end_session();
        m_memoryManager->start_session(core::generate_session_id());
    }
}

void ChatBackend::newChat()
{
    clearConversation();
    setStatusMessage("");
}

QVariantList ChatBackend::getSessions()
{
    QVariantList result;

    if (!m_memoryManager) {
        return result;
    }

    auto sessions = m_memoryManager->list_sessions();
    for (const auto& session : sessions) {
        QVariantMap item;
        item["id"] = QString::fromStdString(session.id);
        item["preview"] = QString::fromStdString(session.preview);

        // Format timestamps
        auto created = std::chrono::system_clock::to_time_t(session.created_at);
        auto updated = std::chrono::system_clock::to_time_t(session.updated_at);
        item["createdAt"] = QString::fromStdString(std::ctime(&created)).trimmed();
        item["updatedAt"] = QString::fromStdString(std::ctime(&updated)).trimmed();

        result.append(item);
    }

    return result;
}

bool ChatBackend::switchSession(const QString& sessionId)
{
    if (!m_memoryManager || m_isBusy) {
        return false;
    }

    // End current session
    m_memoryManager->end_session();

    // Clear UI messages
    m_messages->clear();

    // Resume the selected session
    auto result = m_memoryManager->resume_session(sessionId.toStdString());
    if (result.is_err()) {
        // Failed to resume - start a new session
        m_memoryManager->start_session(core::generate_session_id());
        emit errorOccurred(QString::fromStdString(result.error().message));
        return false;
    }

    // Load messages from thread memory into UI
    auto& thread = m_memoryManager->thread_memory();
    for (const auto& msg : thread.messages()) {
        if (msg.role == core::Role::User) {
            m_messages->addUserMessage(QString::fromStdString(msg.content));
        } else if (msg.role == core::Role::Assistant) {
            m_messages->addAssistantMessage(QString::fromStdString(msg.content));
        }
    }

    return true;
}

void ChatBackend::setCurrentModel(const QString& model)
{
    if (m_currentModel != model) {
        m_currentModel = model;
        // Update config if needed
        if (m_config) {
            m_config->llm.primary_model = model.toStdString();
        }
        emit currentModelChanged();
    }
}

void ChatBackend::onStreamingChunk(const QString& chunk)
{
    m_messages->appendToStreaming(chunk);
}

void ChatBackend::onResponseComplete(const QString& response)
{
    Q_UNUSED(response);
    m_messages->endStreaming();
    setBusy(false);
    setStatusMessage("");
}

void ChatBackend::onError(const QString& message)
{
    m_messages->endStreaming();
    setBusy(false);
    setStatusMessage("");
    emit errorOccurred(message);
}

void ChatBackend::onAgentEvent(int eventType, const QString& message)
{
    auto event = static_cast<agent::AgentEvent>(eventType);

    switch (event) {
    case agent::AgentEvent::Thinking:
        setStatusMessage("Thinking...");
        break;
    case agent::AgentEvent::ToolSelected:
        setStatusMessage(QString("Using tool: %1").arg(message));
        break;
    case agent::AgentEvent::ToolExecuting:
        setStatusMessage(QString("Executing: %1").arg(message));
        break;
    case agent::AgentEvent::ToolCompleted:
        setStatusMessage("Tool completed");
        break;
    case agent::AgentEvent::ToolFailed:
        setStatusMessage(QString("Tool failed: %1").arg(message));
        break;
    case agent::AgentEvent::ResponseReady:
        setStatusMessage("");
        break;
    default:
        break;
    }
}

void ChatBackend::setStatusMessage(const QString& message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}

void ChatBackend::setBusy(bool busy)
{
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit busyChanged();
    }
}

}  // namespace gpagent::ui
