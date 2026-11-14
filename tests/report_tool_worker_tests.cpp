#include "tigersheets/report_worker.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
using namespace tigersheets;
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir directory;const auto program=directory.filePath("tool-worker");QFile file(program);if(!file.open(QIODevice::WriteOnly))return 1;file.write(R"PY(#!/usr/bin/python3
import json,sys
args=dict(zip(sys.argv[1::2],sys.argv[2::2]))
with open(args['--request']) as source: request=json.load(source)
result={'version':1,'revision':request['workbook']['revision'],'contextGeneration':request['contextGeneration'],'status':'ok','diagnostics':[],'toolReply':{'version':'workbook.v1'},'operations':[]}
with open(args['--response'],'w') as destination: json.dump(result,destination)
)PY");file.close();file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);ReportWorker worker;Json result;worker.onResult=[&](Json value){result=std::move(value);};const Json request={{"version",1},{"kind","workbook_tool"},{"workbook",{{"revision",7}}},{"contextGeneration",13},{"retainedLinks",Json::array()},{"tool","workbook_v1_capabilities"},{"args",Json::object()},{"previousProposal",Json::array()}};if(!worker.start(std::make_shared<const Json>(request),program)){std::cerr<<"isolated workbook tools must use the cancellable report process: "<<worker.error().toStdString()<<'\n';return 1;}QElapsedTimer timer;timer.start();while(worker.running()&&timer.elapsed()<5000)app.processEvents();if(result.value("toolReply",Json::object()).value("version",std::string())!="workbook.v1"||!worker.error().isEmpty()){std::cerr<<"worker returns a revision/context-bound tool reply: "<<worker.error().toStdString()<<'\n';return 1;}std::cout<<"report tool worker passed\n";}
