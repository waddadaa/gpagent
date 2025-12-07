#pragma once

#include "gpagent/core/types.hpp"

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace gpagent::ui {

struct ChatMessage {
    QString id;
    QString content;
    QString role;  // "user", "assistant", "system", "tool"
    QDateTime timestamp;
    bool isStreaming = false;
    bool isError = false;
    QString toolName;  // For tool messages
};

class MessageModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ContentRole,
        RoleRole,
        TimestampRole,
        IsStreamingRole,
        IsErrorRole,
        ToolNameRole
    };

    explicit MessageModel(QObject* parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Message operations
    Q_INVOKABLE void addMessage(const QString& content, const QString& role);
    Q_INVOKABLE void addUserMessage(const QString& content);
    Q_INVOKABLE void addAssistantMessage(const QString& content);
    Q_INVOKABLE void addToolMessage(const QString& toolName, const QString& content);

    // Streaming support
    Q_INVOKABLE void beginStreaming();
    Q_INVOKABLE void appendToStreaming(const QString& chunk);
    Q_INVOKABLE void endStreaming();

    // Clear all messages
    Q_INVOKABLE void clear();

    // Get message count
    int count() const { return m_messages.size(); }

    // Convert from core::Message
    void addFromCoreMessage(const core::Message& message);

signals:
    void countChanged();

private:
    QVector<ChatMessage> m_messages;
    int m_streamingIndex = -1;

    QString generateId() const;
};

}  // namespace gpagent::ui
