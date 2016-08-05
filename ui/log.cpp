#include "log.h"
#include "ui_log.h"

#include <QDebug>
#include <QMutex>


bool Log::logOpen = false;

Log::Log(QWidget *parent) : QDialog(parent), ui(new Ui::Log)
{
    ui->setupUi(this);
    text = new QString;
    this->bar = this->ui->logText->verticalScrollBar();
    connect(this, SIGNAL(call_write(char*)), this, SLOT(write(char*)), Qt::QueuedConnection);
}

Log::~Log()
{
    delete ui;
}

void Log::on_buttonClose_clicked()
{
    Log::logOpen = false;
    close();
}

void Log::on_buttonClear_clicked()
{
    this->text->clear();
    this->ui->logText->clear();
}


/*
   This is, by far, THE most complicated piece of code I have
   ever written. This is how it works:

   r_printf(char *format, ...) can be called from either C or C++,
   it takes the format and va_list arguments, and formats them. Then,
   it calls write_c, which sends a signal the Logger, which is connected
   to itself, so the Qt scheduler schedules an update to the window by calling write(),
   so that Qt does not freak out. Crazy, right?

*/


void Log::write(char *msg)
{
    this->text->append(msg);
    this->ui->logText->setPlainText(*this->text);
    this->bar->setValue(bar->maximum());
    free(msg);
}

void write_c(Log *ptr, char *msg) {
    emit ptr->call_write(msg);
}

void r_printf(char *format, ...) {

    char *buf = (char*) malloc(4096);
    va_list argList;
    va_start(argList, format);
    vsnprintf(buf, 4096,format, argList);
    va_end(argList);
    write_c(logptr, buf);

}

