#include "gpagent/ui/message_model.hpp"

#include <QUuid>

namespace gpagent::ui {

MessageModel::MessageModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_messages.size();
}

QVariant MessageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return QVariant();
    }

    const auto& message = m_messages[index.row()];

    switch (role) {
    case IdRole:
        return message.id;
    case ContentRole:
        return message.content;
    case RoleRole:
        return message.role;
    case TimestampRole:
        return message.timestamp;
    case IsStreamingRole:
        return message.isStreaming;
    case IsErrorRole:
        return message.isError;
    case ToolNameRole:
        return message.toolName;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    return {
        {IdRole, "messageId"},
        {ContentRole, "content"},
        {RoleRole, "role"},
        {TimestampRole, "timestamp"},
        {IsStreamingRole, "isStreaming"},
        {IsErrorRole, "isError"},
        {ToolNameRole, "toolName"}
    };
}

void MessageModel::addMessage(const QString& content, const QString& role)
{
    ChatMessage msg;
    msg.id = generateId();
    msg.content = content;
    msg.role = role;
    msg.timestamp = QDateTime::currentDateTime();

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msg);
    endInsertRows();

    emit countChanged();
}

void MessageModel::addUserMessage(const QString& content)
{
    addMessage(content, "user");
}

void MessageModel::addAssistantMessage(const QString& content)
{
    addMessage(content, "assistant");
}

void MessageModel::addToolMessage(const QString& toolName, const QString& content)
{
    ChatMessage msg;
    msg.id = generateId();
    msg.content = content;
    msg.role = "tool";
    msg.toolName = toolName;
    msg.timestamp = QDateTime::currentDateTime();

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msg);
    endInsertRows();

    emit countChanged();
}

void MessageModel::beginStreaming()
{
    ChatMessage msg;
    msg.id = generateId();
    msg.content = "";
    msg.role = "assistant";
    msg.timestamp = QDateTime::currentDateTime();
    msg.isStreaming = true;

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msg);
    m_streamingIndex = m_messages.size() - 1;
    endInsertRows();

    emit countChanged();
}

void MessageModel::appendToStreaming(const QString& chunk)
{
    if (m_streamingIndex < 0 || m_streamingIndex >= m_messages.size()) {
        return;
    }

    m_messages[m_streamingIndex].content += chunk;

    QModelIndex idx = index(m_streamingIndex);
    emit dataChanged(idx, idx, {ContentRole});
}

void MessageModel::endStreaming()
{
    if (m_streamingIndex < 0 || m_streamingIndex >= m_messages.size()) {
        return;
    }

    m_messages[m_streamingIndex].isStreaming = false;

    QModelIndex idx = index(m_streamingIndex);
    emit dataChanged(idx, idx, {IsStreamingRole});

    m_streamingIndex = -1;
}

void MessageModel::clear()
{
    if (m_messages.isEmpty()) {
        return;
    }

    beginResetModel();
    m_messages.clear();
    m_streamingIndex = -1;
    endResetModel();

    emit countChanged();
}

void MessageModel::addFromCoreMessage(const core::Message& message)
{
    ChatMessage msg;
    msg.id = generateId();
    msg.content = QString::fromStdString(message.content);
    msg.role = QString::fromStdString(std::string(core::role_to_string(message.role)));
    msg.timestamp = QDateTime::fromSecsSinceEpoch(
        std::chrono::duration_cast<std::chrono::seconds>(
            message.timestamp.time_since_epoch()).count());

    if (message.name) {
        msg.toolName = QString::fromStdString(*message.name);
    }

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msg);
    endInsertRows();

    emit countChanged();
}

QString MessageModel::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace gpagent::ui
