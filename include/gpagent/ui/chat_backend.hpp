#pragma once

#include "gpagent/agent/orchestrator.hpp"
#include "gpagent/core/config.hpp"
#include "gpagent/ui/message_model.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <QVariantList>
#include <memory>

namespace gpagent::ui {

// Worker for async LLM operations
class ChatWorker : public QObject {
    Q_OBJECT

public:
    explicit ChatWorker(agent::Orchestrator* orchestrator);

public slots:
    void processMessage(const QString& message);

signals:
    void streamingChunk(const QString& chunk);
    void responseComplete(const QString& response);
    void error(const QString& message);
    void agentEvent(int eventType, const QString& message);

private:
    agent::Orchestrator* m_orchestrator;
};

// Main chat backend exposed to QML
class ChatBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(MessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString currentModel READ currentModel WRITE setCurrentModel NOTIFY currentModelChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit ChatBackend(QObject* parent = nullptr);
    ~ChatBackend() override;

    // Initialize with config
    Q_INVOKABLE bool initialize(const QString& configPath = QString());

    // Send a message
    Q_INVOKABLE void sendMessage(const QString& content);

    // Stop current generation
    Q_INVOKABLE void stopGeneration();

    // Clear conversation
    Q_INVOKABLE void clearConversation();

    // Create new chat session
    Q_INVOKABLE void newChat();

    // Get list of available sessions (returns JSON array)
    Q_INVOKABLE QVariantList getSessions();

    // Switch to a specific session
    Q_INVOKABLE bool switchSession(const QString& sessionId);

    // Properties
    MessageModel* messages() const { return m_messages; }
    bool isBusy() const { return m_isBusy; }
    QString currentModel() const { return m_currentModel; }
    void setCurrentModel(const QString& model);
    QString statusMessage() const { return m_statusMessage; }

signals:
    void busyChanged();
    void currentModelChanged();
    void statusMessageChanged();
    void errorOccurred(const QString& message);
    void initialized();

private slots:
    void onStreamingChunk(const QString& chunk);
    void onResponseComplete(const QString& response);
    void onError(const QString& message);
    void onAgentEvent(int eventType, const QString& message);

private:
    void setupWorker();
    void setStatusMessage(const QString& message);
    void setBusy(bool busy);

    MessageModel* m_messages = nullptr;
    bool m_isBusy = false;
    QString m_currentModel = "claude-opus-4-5-20251101";
    QString m_statusMessage;

    // Backend components
    std::unique_ptr<core::Config> m_config;
    std::unique_ptr<llm::LLMGateway> m_llmGateway;
    std::unique_ptr<tools::ToolRegistry> m_toolRegistry;
    std::unique_ptr<tools::ToolExecutor> m_toolExecutor;
    std::unique_ptr<memory::MemoryManager> m_memoryManager;
    std::unique_ptr<context::ContextManager> m_contextManager;
    std::unique_ptr<agent::Orchestrator> m_orchestrator;

    // Worker thread
    QThread* m_workerThread = nullptr;
    ChatWorker* m_worker = nullptr;
};

}  // namespace gpagent::ui
