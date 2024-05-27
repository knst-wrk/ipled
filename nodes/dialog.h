/** This file is part of ipled - a versatile LED strip controller.
Copyright (C) 2024 Sven Pauli <sven@knst-wrk.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef DIALOG_H
#define DIALOG_H

#include "ui_dialog.h"

#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QLinkedList>

class QByteArray;
class QSerialPort;
class QTreeWidgetItem;
class QAbstractItemModel;

class Dialog;

class Dialog:
    public QDialog,
    private Ui::Dialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();

    void sendFrame(const QByteArray &frame);

    enum {
        IdColumn = 0,
        BatteryColumn = 1,
        RssiColumn = 2,
        TemperatureColumn = 3,
        QosColumn = 4
    } Columns;

    QAbstractItemModel *model() const;


    static Dialog *instance()
    {
        return m_instance;
    }

public slots:
    Q_SCRIPTABLE void startScene(int id, int scene);
    Q_SCRIPTABLE void stopScene(int node);

protected:
    void changeEvent(QEvent *e);

private:
    static Dialog *m_instance;

    QSerialPort *port;
    QString dataRead;

    QTimer *idleTimer;
    int idleIndex;

    class NodeItem;
    bool addNode(int id);
    QMap<int, NodeItem *> nodes;

    class Task;
    class PingTask;
    class SceneTask;
    class NarcoticTask;
    class SleepTask;
    class WakeTask;
    class DimTask;
    class FrameTask;
    QLinkedList<Task *> tasks;
    Task *currentTask;
    QTimer *timeoutTimer;
    void postTask(Task *task);
    Task *popTask();


private slots:
    void connectToggled(bool checked);
    void refreshPorts();

    void addNode();
    void addNodes();
    void removeNode();

    void sleepTask();
    void wakeTask();
    void startTask();
    void pauseTask();
    void stopTask();
    void skipTask();
    void dimTask();
    void frameTask();
    void matrixTask();

    void idle();
    void timeout();

    void log(const QString &text, const QString prefix = QString());
    void readReady();
};

#endif // DIALOG_H
