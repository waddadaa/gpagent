#include "gpagent/ui/config_manager.hpp"

#include <QStandardPaths>
#include <QDir>
#include <QProcessEnvironment>

namespace gpagent::ui {

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    // Set default config path
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    m_configPath = configDir + "/gpagent/config.yaml";
}

bool ConfigManager::load(const QString& path)
{
    QString loadPath = path.isEmpty() ? m_configPath : path;

    auto result = core::Config::load(loadPath.toStdString());
    if (result.is_ok()) {
        m_config = result.value();
        m_config.expand_paths();
        m_configPath = loadPath;
        clearDirty();

        // Emit all property changes
        emit claudeApiKeyChanged();
        emit geminiApiKeyChanged();
        emit openaiApiKeyChanged();
        emit perplexityApiKeyChanged();
        emit tavilyApiKeyChanged();
        emit googleSearchApiKeyChanged();
        emit googleSearchCxChanged();
        emit searchProviderChanged();
        emit primaryProviderChanged();
        emit primaryModelChanged();
        emit temperatureChanged();

        return true;
    } else {
        // Try to load from environment variables
        auto env = QProcessEnvironment::systemEnvironment();

        if (env.contains("ANTHROPIC_API_KEY")) {
            m_config.api_keys.anthropic = env.value("ANTHROPIC_API_KEY").toStdString();
        }
        if (env.contains("GOOGLE_API_KEY")) {
            m_config.api_keys.google = env.value("GOOGLE_API_KEY").toStdString();
        }
        if (env.contains("OPENAI_API_KEY")) {
            m_config.api_keys.openai = env.value("OPENAI_API_KEY").toStdString();
        }

        emit loadError(QString::fromStdString(result.error().message));
        return false;
    }
}

bool ConfigManager::save()
{
    // Ensure directory exists
    QDir dir(QFileInfo(m_configPath).absolutePath());
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto result = m_config.save(m_configPath.toStdString());
    if (result.is_ok()) {
        clearDirty();
        emit saved();
        return true;
    } else {
        emit saveError(QString::fromStdString(result.error().message));
        return false;
    }
}

void ConfigManager::reset()
{
    m_config = core::Config();
    markDirty();

    emit claudeApiKeyChanged();
    emit geminiApiKeyChanged();
    emit openaiApiKeyChanged();
    emit perplexityApiKeyChanged();
    emit tavilyApiKeyChanged();
    emit googleSearchApiKeyChanged();
    emit googleSearchCxChanged();
    emit searchProviderChanged();
    emit primaryProviderChanged();
    emit primaryModelChanged();
    emit temperatureChanged();
}

bool ConfigManager::validateClaudeKey()
{
    // Basic validation - check if key looks like an Anthropic key
    QString key = claudeApiKey();
    return key.startsWith("sk-ant-") && key.length() > 20;
}

bool ConfigManager::validateGeminiKey()
{
    QString key = geminiApiKey();
    return key.length() > 20;
}

bool ConfigManager::validateOpenaiKey()
{
    QString key = openaiApiKey();
    return key.startsWith("sk-") && key.length() > 20;
}

QString ConfigManager::claudeApiKey() const
{
    return QString::fromStdString(m_config.api_keys.anthropic);
}

void ConfigManager::setClaudeApiKey(const QString& key)
{
    if (claudeApiKey() != key) {
        m_config.api_keys.anthropic = key.toStdString();
        markDirty();
        emit claudeApiKeyChanged();
    }
}

QString ConfigManager::geminiApiKey() const
{
    return QString::fromStdString(m_config.api_keys.google);
}

void ConfigManager::setGeminiApiKey(const QString& key)
{
    if (geminiApiKey() != key) {
        m_config.api_keys.google = key.toStdString();
        markDirty();
        emit geminiApiKeyChanged();
    }
}

QString ConfigManager::openaiApiKey() const
{
    return QString::fromStdString(m_config.api_keys.openai);
}

void ConfigManager::setOpenaiApiKey(const QString& key)
{
    if (openaiApiKey() != key) {
        m_config.api_keys.openai = key.toStdString();
        markDirty();
        emit openaiApiKeyChanged();
    }
}

// Search API Keys
QString ConfigManager::perplexityApiKey() const
{
    return QString::fromStdString(m_config.api_keys.perplexity);
}

void ConfigManager::setPerplexityApiKey(const QString& key)
{
    if (perplexityApiKey() != key) {
        m_config.api_keys.perplexity = key.toStdString();
        markDirty();
        emit perplexityApiKeyChanged();
    }
}

QString ConfigManager::tavilyApiKey() const
{
    return QString::fromStdString(m_config.api_keys.tavily);
}

void ConfigManager::setTavilyApiKey(const QString& key)
{
    if (tavilyApiKey() != key) {
        m_config.api_keys.tavily = key.toStdString();
        markDirty();
        emit tavilyApiKeyChanged();
    }
}

QString ConfigManager::googleSearchApiKey() const
{
    return QString::fromStdString(m_config.api_keys.google_search);
}

void ConfigManager::setGoogleSearchApiKey(const QString& key)
{
    if (googleSearchApiKey() != key) {
        m_config.api_keys.google_search = key.toStdString();
        markDirty();
        emit googleSearchApiKeyChanged();
    }
}

QString ConfigManager::googleSearchCx() const
{
    return QString::fromStdString(m_config.api_keys.google_cx);
}

void ConfigManager::setGoogleSearchCx(const QString& cx)
{
    if (googleSearchCx() != cx) {
        m_config.api_keys.google_cx = cx.toStdString();
        markDirty();
        emit googleSearchCxChanged();
    }
}

// Search Settings
QString ConfigManager::searchProvider() const
{
    return QString::fromStdString(m_config.search.provider);
}

void ConfigManager::setSearchProvider(const QString& provider)
{
    if (searchProvider() != provider) {
        m_config.search.provider = provider.toStdString();
        markDirty();
        emit searchProviderChanged();
    }
}

QString ConfigManager::primaryProvider() const
{
    return QString::fromStdString(m_config.llm.primary_provider);
}

void ConfigManager::setPrimaryProvider(const QString& provider)
{
    if (primaryProvider() != provider) {
        m_config.llm.primary_provider = provider.toStdString();
        markDirty();
        emit primaryProviderChanged();
    }
}

QString ConfigManager::primaryModel() const
{
    return QString::fromStdString(m_config.llm.primary_model);
}

void ConfigManager::setPrimaryModel(const QString& model)
{
    if (primaryModel() != model) {
        m_config.llm.primary_model = model.toStdString();
        markDirty();
        emit primaryModelChanged();
    }
}

double ConfigManager::temperature() const
{
    return m_config.llm.temperature;
}

void ConfigManager::setTemperature(double temp)
{
    if (qAbs(temperature() - temp) > 0.001) {
        m_config.llm.temperature = temp;
        markDirty();
        emit temperatureChanged();
    }
}

void ConfigManager::markDirty()
{
    if (!m_isDirty) {
        m_isDirty = true;
        emit isDirtyChanged();
    }
}

void ConfigManager::clearDirty()
{
    if (m_isDirty) {
        m_isDirty = false;
        emit isDirtyChanged();
    }
}

QString ConfigManager::maskApiKey(const QString& key) const
{
    if (key.length() <= 8) {
        return QString(key.length(), '*');
    }
    return key.left(4) + QString(key.length() - 8, '*') + key.right(4);
}

}  // namespace gpagent::ui
