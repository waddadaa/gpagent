#include "gpagent/ui/chat_backend.hpp"
#include "gpagent/ui/config_manager.hpp"
#include "gpagent/ui/message_model.hpp"

#include <QtQml>

namespace gpagent::ui {

void registerQmlTypes()
{
    qmlRegisterType<ChatBackend>("GPAgent", 1, 0, "ChatBackend");
    qmlRegisterType<ConfigManager>("GPAgent", 1, 0, "ConfigManager");
    qmlRegisterType<MessageModel>("GPAgent", 1, 0, "MessageModel");
}

}  // namespace gpagent::ui
