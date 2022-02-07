#include "csrmap.h"
#include <QFile>
#include <QTextStream>

CSRMap::CSRMap(QObject *parent) : QObject(parent)
{
    readCSRFile();
}

void CSRMap::readCSRFile()
{
    QFile csv("csr.csv");
    auto openResult = csv.open(QFile::ReadOnly | QFile::Text);
    if(openResult) {
        bool CLK_GATE_OK = false;
        bool CPU_RESET_OK = false;
        QTextStream in(&csv);
        while (!in.atEnd()){
          QString s=in.readLine(); // reads line from file
          if(s.contains("rgb_control_csr")) {
              QStringList details = s.split(',');
              QString hexAddr = details[2].last(8);
              int value = hexAddr.toUInt(&CLK_GATE_OK, 16);
              if(CLK_GATE_OK) {
                  if (value == 0) {
                      CLK_GATE_OK = false;
                  } else {
                      this->CLK_GATE = value;
                  }
              }
          }
          if(s.contains("ctrl_reset")) {
              QStringList details = s.split(',');
              QString hexAddr = details[2].last(8);
              int value = hexAddr.toUInt(&CPU_RESET_OK, 16);
              if(CPU_RESET_OK) {
                  if (value == 0) {
                      CPU_RESET_OK = false;
                  } else {
                      this->CPU_RESET = value;
                  }
              }
          }
        }
    }
}
