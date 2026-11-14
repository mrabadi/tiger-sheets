#include "tigersheets/codex.hpp"
#include "tigersheets/main_window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace tigersheets;
namespace {
void require(bool condition, const char* message) {
 if (!condition) throw std::runtime_error(message);
}
bool until(const std::function<bool()>& predicate) {
 QElapsedTimer elapsed; elapsed.start();
 while (!predicate() && elapsed.elapsed() < 5000)
  QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
 return predicate();
}
QString fakeRuntime(QTemporaryDir& directory) {
 const auto path = directory.filePath("codex-panel-fake");
 QFile script(path); require(script.open(QIODevice::WriteOnly), "cannot create deterministic Codex runtime");
 script.write(R"PY(#!/usr/bin/python3
import json, sys
log = open(__file__ + '.log', 'a', buffering=1)
turn = 0
for line in sys.stdin:
    message = json.loads(line)
    log.write(json.dumps(message) + '\n')
    method = message.get('method')
    if method == 'initialized' or method is None:
        continue
    if method == 'initialize':
        result = {'userAgent': 'TigerSheets deterministic panel test', 'platformOs': 'linux'}
    elif method == 'config/read':
        result = {'config': {'mcp_servers': {}}}
    elif method == 'account/read':
        result = {'account': {'type': 'chatgpt'}, 'requiresOpenaiAuth': True}
    elif method == 'model/list':
        result = {'data': [
            {'id': 'other-id', 'model': 'other-model', 'isDefault': True,
             'supportedReasoningEfforts': ['high'], 'serviceTiers': ['standard']},
            {'id': 'sol-id', 'model': 'gpt-6.1-sol', 'isDefault': False,
             'supportedReasoningEfforts': ['ultra', 'medium', 'high', 'low', 'xhigh'],
             'serviceTiers': [{'id': 'fast', 'name': 'Fast', 'description': 'May use additional credits'}]}
        ], 'nextCursor': None}
    elif method in ('thread/start', 'thread/resume'):
        result = {'thread': {'id': 'panel-thread'}}
    elif method == 'turn/start':
        turn += 1
        result = {'turn': {'id': 'panel-turn-' + str(turn), 'status': 'inProgress'}}
    elif method == 'turn/interrupt':
        print(json.dumps({'id': message['id'], 'result': {}}), flush=True)
        print(json.dumps({'method': 'turn/completed', 'params': {
            'threadId': 'panel-thread', 'turn': {'id': 'panel-turn-' + str(turn), 'status': 'interrupted'}}}), flush=True)
        continue
    else:
        result = {}
    print(json.dumps({'id': message['id'], 'result': result}), flush=True)
)PY");
 script.close();
 require(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner), "cannot make fake runtime executable");
 return path;
}
Json messages(const QString& script) {
 QFile log(script + ".log");
 if (!log.exists()) return Json::array();
 require(log.open(QIODevice::ReadOnly), "cannot read panel protocol log");
 Json result = Json::array();
 const auto bytes=log.readAll();
 // A worker can be midway through writing a large tool-schema log entry.
 for (const auto& line : bytes.left(bytes.lastIndexOf('\n')+1).split('\n'))
  if (!line.isEmpty()) result.push_back(Json::parse(line.toStdString()));
 return result;
}
Json requests(const QString& script, const char* method) {
 Json result = Json::array();
 for (const auto& message : messages(script))
  if (message.value("method", std::string()) == method) result.push_back(message.at("params"));
 return result;
}
CodexClient* clientFor(MainWindow& window) {
 for (auto* child : window.children())
  if (auto* client = dynamic_cast<CodexClient*>(child)) return client;
 throw std::runtime_error("workspace has no Codex client");
}
template <typename Widget> Widget* control(MainWindow& window, const char* name) {
 auto* result = window.findChild<Widget*>(QString::fromLatin1(name));
 require(result, name);
 return result;
}
void select(QComboBox* choice, const char* wireValue) {
 const int index = choice->findData(QString::fromLatin1(wireValue));
 require(index >= 0, "expected runtime-advertised option is missing");
 choice->setCurrentIndex(index);
}
void showPanel(MainWindow& window) {
 window.resize(1280, 800);
 window.show();
 control<QDockWidget>(window, "workspace.codex")->show();
 QCoreApplication::processEvents();
}
void suiteSettingsReachTheRuntime() {
 QTemporaryDir directory;
 const auto script = fakeRuntime(directory);
 MainWindow window;
 showPanel(window);
 auto* model = control<QComboBox>(window, "codex.model");
 auto* effort = control<QComboBox>(window, "codex.effort");
 auto* speed = control<QComboBox>(window, "codex.speed");
 auto* input = control<QPlainTextEdit>(window, "codex.input");
 auto* send = control<QPushButton>(window, "codex.send");
 auto* connection = control<QPushButton>(window, "codex.connection");
 auto* stop = control<QPushButton>(window, "codex.stop");
 auto* client = clientFor(window);
 require(!model->isEnabled() && !effort->isEnabled() && !speed->isEnabled() &&
         !input->isEnabled() && !send->isEnabled(), "offline Codex controls must be disabled");
 client->connectRuntime(script);
 require(until([&] { return client->isReady(); }), "workspace failed to connect to deterministic runtime");
 require(model->currentData().toString() == "gpt-6.1-sol" && model->currentText() == "GPT-6.1 Sol",
         "workspace did not select the advertised suite model");
 require(effort->currentText() == "Light" && effort->currentData().toString() == "low" &&
         speed->currentText() == "Standard" && speed->currentData().toString().isEmpty(),
         "workspace did not use OwlDocs Light / Standard defaults");
 require(input->isEnabled() && send->isEnabled() && !stop->isVisible() && connection->text() == "Disconnect",
         "connected Codex interaction state differs from the suite");
 select(effort, "ultra"); select(speed, "fast");
 require(!client->isBusy() && requests(script, "thread/start").empty() && requests(script, "turn/start").empty(),
         "changing model controls must not send workbook context");
 input->setPlainText("Review this forecast using my selected options");
 QTest::mouseClick(send, Qt::LeftButton);
 require(until([&] { return requests(script, "turn/start").size() == 1; }), "Send did not start a Codex turn");
 const auto first = requests(script, "turn/start").at(0);
 require(first.value("model", std::string()) == "gpt-6.1-sol" &&
         first.value("effort", std::string()) == "ultra" &&
         first.value("serviceTierForTurn", std::string()) == "fast",
         "workspace choices did not reach the actual Codex request");
 require(client->isBusy() && !input->isEnabled() && !send->isEnabled() && stop->isVisible() &&
         model->isEnabled() && effort->isEnabled() && speed->isEnabled(),
         "busy Codex should preserve next-turn choices while disabling the composer");
 select(model, "other-model");
 require(effort->count() == 1 && effort->currentData().toString() == "high" &&
         speed->count() == 1 && speed->currentData().toString() == "standard",
         "changing models did not reset to its advertised reasoning and speed");
 require(requests(script, "turn/start").size() == 1,
         "next-turn setting changes started or replaced an active turn");
 QTest::mouseClick(stop, Qt::LeftButton);
 require(until([&] { return !client->isBusy(); }), "Stop did not cancel the active Codex turn");
 require(requests(script, "turn/interrupt").size() == 1 && input->isEnabled() && send->isEnabled() && !stop->isVisible(),
         "cancelled Codex turn did not restore the composer");
 input->setPlainText("Use the next model now");
 QTest::mouseClick(send, Qt::LeftButton);
 require(until([&] { return requests(script, "turn/start").size() == 2; }), "second Codex request was not sent");
 const auto second = requests(script, "turn/start").at(1);
 require(second.value("model", std::string()) == "other-model" &&
         second.value("effort", std::string()) == "high" &&
         second.value("serviceTierForTurn", std::string()) == "standard",
         "next turn did not apply the changed model, reasoning and Standard speed");
 require(requests(script, "thread/start").size() == 1, "changing next-turn settings discarded workbook conversation");
 QTest::mouseClick(stop, Qt::LeftButton);
 require(until([&] { return !client->isBusy(); }), "second Codex turn did not cancel");
 QTest::mouseClick(connection, Qt::LeftButton);
 require(!client->isReady() && !model->isEnabled() && !effort->isEnabled() && !speed->isEnabled() &&
         !input->isEnabled() && !send->isEnabled() && connection->text() == "Reconnect",
         "Disconnect did not restore the suite offline controls");
}
void suiteComposerKeyboardBehavior() {
 QTemporaryDir directory;
 const auto script = fakeRuntime(directory);
 MainWindow window;
 showPanel(window);
 auto* client = clientFor(window);
 client->connectRuntime(script);
 require(until([&] { return client->isReady(); }), "keyboard test runtime did not connect");
 auto* input = control<QPlainTextEdit>(window, "codex.input");
 input->setFocus();
 QTest::keyClicks(input, "First line");
 QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier);
 QTest::keyClicks(input, "Second line");
 require(input->toPlainText() == "First line\nSecond line" && !client->isBusy(),
         "Shift+Enter should insert a newline without sending workbook data");
 QTest::keyClick(input, Qt::Key_Return);
 require(until([&] { return requests(script, "turn/start").size() == 1; }), "Enter did not send the Codex message");
 require(input->toPlainText().isEmpty(), "sent composer message was not cleared");
 const auto text = requests(script, "turn/start").at(0).at("input").at(0).at("text").get<std::string>();
 require(text.find("First line\nSecond line") != std::string::npos, "keyboard send changed the composed message");
 QTest::mouseClick(control<QPushButton>(window, "codex.stop"), Qt::LeftButton);
 require(until([&] { return !client->isBusy(); }), "keyboard test turn did not stop");
}
}
int main(int argc, char** argv) {
 QApplication app(argc, argv);
 QTemporaryDir stateDirectory;
 qputenv("TIGERSHEETS_STATE_DIR", stateDirectory.path().toUtf8());
 try {
  const QString scenario = argc > 1 ? QString::fromLocal8Bit(argv[1]) : "selectors";
  if (scenario == "selectors") suiteSettingsReachTheRuntime();
  else if (scenario == "keyboard") suiteComposerKeyboardBehavior();
  else throw std::runtime_error("unknown Codex panel integration scenario");
  std::cout << "Codex panel " << scenario.toStdString() << " integration passed\n";
 } catch (const std::exception& error) {
  std::cerr << "FAILED: " << error.what() << '\n';
  return 1;
 }
}
