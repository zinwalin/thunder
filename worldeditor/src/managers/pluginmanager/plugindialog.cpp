#include <QFileDialog>

#include "ui_plugindialog.h"

#include "plugindialog.h"

#include <editor/pluginmanager.h>

PluginDialog::PluginDialog(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::PluginDialog) {

    ui->setupUi(this);

    setWindowFlags(windowFlags() ^ Qt::WindowContextHelpButtonHint);

    ui->tableView->setModel(PluginManager::instance());
    ui->tableView->horizontalHeader()->setHighlightSections(false);
}

PluginDialog::~PluginDialog() {
    delete ui;
}

void PluginDialog::on_closeButton_clicked() {
    accept();
}

void PluginDialog::on_loadButton_clicked() {
    QDir dir = QDir(QDir::currentPath());
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Please select Thunder Engine Mod"),
                                                dir.absolutePath(),
                                                tr("Mods (*.dll *.mod)") );
    if(!path.isEmpty()) {
        PluginManager *model = PluginManager::instance();
        if(model->loadPlugin(dir.relativeFilePath(path))) {
            ui->tableView->setModel(model);
        }
    }
}

void PluginDialog::changeEvent(QEvent *event) {
    if(event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
}
