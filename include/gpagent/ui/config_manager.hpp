#pragma once

#include "gpagent/core/config.hpp"

#include <QObject>
#include <QString>

namespace gpagent::ui {

class ConfigManager : public QObject {
    Q_OBJECT

    // API Keys
    Q_PROPERTY(QString claudeApiKey READ claudeApiKey WRITE setClaudeApiKey NOTIFY claudeApiKeyChanged)
    Q_PROPERTY(QString geminiApiKey READ geminiApiKey WRITE setGeminiApiKey NOTIFY geminiApiKeyChanged)
    Q_PROPERTY(QString openaiApiKey READ openaiApiKey WRITE setOpenaiApiKey NOTIFY openaiApiKeyChanged)

    // Search API Keys
    Q_PROPERTY(QString perplexityApiKey READ perplexityApiKey WRITE setPerplexityApiKey NOTIFY perplexityApiKeyChanged)
    Q_PROPERTY(QString tavilyApiKey READ tavilyApiKey WRITE setTavilyApiKey NOTIFY tavilyApiKeyChanged)
    Q_PROPERTY(QString googleSearchApiKey READ googleSearchApiKey WRITE setGoogleSearchApiKey NOTIFY googleSearchApiKeyChanged)
    Q_PROPERTY(QString googleSearchCx READ googleSearchCx WRITE setGoogleSearchCx NOTIFY googleSearchCxChanged)

    // Search Settings
    Q_PROPERTY(QString searchProvider READ searchProvider WRITE setSearchProvider NOTIFY searchProviderChanged)

    // LLM Settings
    Q_PROPERTY(QString primaryProvider READ primaryProvider WRITE setPrimaryProvider NOTIFY primaryProviderChanged)
    Q_PROPERTY(QString primaryModel READ primaryModel WRITE setPrimaryModel NOTIFY primaryModelChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged)

    // Status
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY isDirtyChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)

public:
    explicit ConfigManager(QObject* parent = nullptr);

    // Load/Save
    Q_INVOKABLE bool load(const QString& path = QString());
    Q_INVOKABLE bool save();
    Q_INVOKABLE void reset();

    // Validate API keys
    Q_INVOKABLE bool validateClaudeKey();
    Q_INVOKABLE bool validateGeminiKey();
    Q_INVOKABLE bool validateOpenaiKey();

    // Get underlying config for backend use
    const core::Config& config() const { return m_config; }
    core::Config& config() { return m_config; }

    // API Key getters/setters
    QString claudeApiKey() const;
    void setClaudeApiKey(const QString& key);

    QString geminiApiKey() const;
    void setGeminiApiKey(const QString& key);

    QString openaiApiKey() const;
    void setOpenaiApiKey(const QString& key);

    // Search API Key getters/setters
    QString perplexityApiKey() const;
    void setPerplexityApiKey(const QString& key);

    QString tavilyApiKey() const;
    void setTavilyApiKey(const QString& key);

    QString googleSearchApiKey() const;
    void setGoogleSearchApiKey(const QString& key);

    QString googleSearchCx() const;
    void setGoogleSearchCx(const QString& cx);

    // Search Settings getters/setters
    QString searchProvider() const;
    void setSearchProvider(const QString& provider);

    // LLM Settings getters/setters
    QString primaryProvider() const;
    void setPrimaryProvider(const QString& provider);

    QString primaryModel() const;
    void setPrimaryModel(const QString& model);

    double temperature() const;
    void setTemperature(double temp);

    // Status
    bool isDirty() const { return m_isDirty; }
    QString configPath() const { return m_configPath; }

signals:
    void claudeApiKeyChanged();
    void geminiApiKeyChanged();
    void openaiApiKeyChanged();
    void perplexityApiKeyChanged();
    void tavilyApiKeyChanged();
    void googleSearchApiKeyChanged();
    void googleSearchCxChanged();
    void searchProviderChanged();
    void primaryProviderChanged();
    void primaryModelChanged();
    void temperatureChanged();
    void isDirtyChanged();
    void saved();
    void loadError(const QString& message);
    void saveError(const QString& message);

private:
    void markDirty();
    void clearDirty();
    QString maskApiKey(const QString& key) const;

    core::Config m_config;
    QString m_configPath;
    bool m_isDirty = false;
};

}  // namespace gpagent::ui
