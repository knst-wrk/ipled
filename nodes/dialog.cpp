/** This file is part of ipled - a versatile LED stripe controller.
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

#include <QtDebug>

#include <QtCore/QTimer>
#include <QtCore/QLocale>
#include <QtCore/QSettings>
#include <QtCore/QTextStream>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QListWidgetItem>
#include <QtDBus/QtDBus>

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

#include "frame.h"
#include "matrix.h"
#include "dialog.h"

Dialog *Dialog::m_instance = 0;


class Dialog::NodeItem:
    public QTreeWidgetItem
{
public:
    NodeItem(int id):
        QTreeWidgetItem(),

        m_id(id),
        m_asleep(false),
        m_qos(50)
    {
        setText(Dialog::IdColumn, QLocale().toString(id));
        setIcon(Dialog::IdColumn, QIcon(":/offline.png"));
    }

    ~NodeItem();

    int id() const { return m_id; }

    bool isAsleep() const { return m_asleep; }
    void sleep()
    {
        m_asleep = true;
        setIcon(Dialog::IdColumn, QIcon(":/asleep.png"));
    }
    void wake()
    {
        m_asleep = false;
    }


    void attention()
    {
        setIcon(Dialog::IdColumn, QIcon(":/task-attention.png"));
    }

    void goodQos()
    {
        m_qos = qMin(m_qos + 1, 100);
        setText(Dialog::QosColumn, QLocale().toString(m_qos));
        setBackgroundColor(Dialog::QosColumn, QColor::fromRgbF((100 - m_qos)/100.0, m_qos/100.0, 0));
    }

    void badQos()
    {
        m_qos = qMax(m_qos -10, 0);
        setText(Dialog::QosColumn, QLocale().toString(m_qos));
        setBackgroundColor(Dialog::QosColumn, QColor::fromRgbF((100 - m_qos)/100.0, m_qos/100.0, 0));
    }

    void store(QSettings &settings) const
    {
        settings.setValue("id", m_id);
    }

private:
    int m_id;
    bool m_asleep;
    int m_qos;
};

Dialog::NodeItem::~NodeItem()
{

}


class Dialog::Task
{
public:
    Task(NodeItem *node, int ttl = 4):
        m_node(node),
        m_ttl(ttl)
    {

    }

    virtual ~Task();

    NodeItem *node() const { return m_node; }
    int ttl() const { return m_ttl; }

    virtual void request(QTextStream &stream) = 0;
    virtual void response(QTextStream &stream)
    {
        QString s = stream.readAll();
        if (s.startsWith("100")) {
            node()->goodQos();
        }
        else {
            node()->attention();
            node()->badQos();
        }
    }

    virtual void timeout()
    {
        node()->badQos();
        if (m_ttl)
            m_ttl--;
    }

private:
    NodeItem *m_node;
    int m_ttl;
};

Dialog::Task::~Task()
{

}


class Dialog::PingTask:
    public Task
{
public:
    PingTask(NodeItem *node):
        Task(node, 1)
    {

    }

    ~PingTask();

    void request(QTextStream &stream)
    {
        stream << "PING " << node()->id()
            << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
    }

    void response(QTextStream &stream)
    {
        QString s = stream.readAll();
        if (s.startsWith("100")) {
            node()->setIcon(Dialog::IdColumn, QIcon(":/online.png"));
            node()->goodQos();
            QStringList lines = s.split('\n');
            for (QString line: lines) {
                const QString arg = line.mid(line.indexOf(':') + 1);
                if (line.startsWith("Vbat")) {
                    node()->setText(Dialog::BatteryColumn, QLocale().toString(arg.toDouble() / 1000.0, 'f', 2) + "V");
                }
                else if (line.startsWith("Rssi")) {
                    node()->setText(Dialog::RssiColumn, QLocale().toString(arg.toDouble(), 'f', 0) + "dB");
                }
                else if (line.startsWith("Temperature")) {
                    node()->setText(Dialog::TemperatureColumn, QLocale().toString(arg.toDouble(), 'f', 0) + "°C");
                }
            }
        }
        else {
            node()->setIcon(Dialog::IdColumn, QIcon(":/offline.png"));
            node()->badQos();
        }
    }
};

Dialog::PingTask::~PingTask()
{

}


class Dialog::SceneTask:
    public Task
{
public:
    SceneTask(NodeItem *node):
        Task(node),

        m_scene(0),
        m_mode(start_mode)
    {

    }

    ~SceneTask();

    void setScene(int scene)
    {
        if (scene >= 0 && scene < 0xFFFF) {
            m_scene = scene;
            m_mode = start_mode;
        }
    }

    int scene() const
    {
        if (m_mode == start_mode)
            return m_scene;
        else
            return -1;
    }

    void stop()
    {
        m_mode = stop_mode;
    }

    bool isStop() const
    {
        return m_mode == stop_mode;
    }

    void pause()
    {
        m_mode = pause_mode;
    }

    bool isPause() const
    {
        return m_mode == pause_mode;
    }

    void skip()
    {
        m_mode = skip_mode;
    }

    bool isSkip() const
    {
        return m_mode == skip_mode;
    }

    void request(QTextStream &stream)
    {
        switch (m_mode)
        {
        case start_mode:
            stream << "START " << node()->id() << " " << m_scene
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
            break;

        case stop_mode:
            stream << "STOP " << node()->id()
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
            break;

        case pause_mode:
            stream << "PAUSE " << node()->id()
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
            break;

        case skip_mode:
            stream << "SKIP " << node()->id()
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
            break;
        }
    }

private:
    int m_scene;
    enum Mode {
        stop_mode,
        skip_mode,
        start_mode,
        pause_mode,
    } m_mode;
};

Dialog::SceneTask::~SceneTask()
{

}


class Dialog::NarcoticTask:
    public Task
{
public:
    ~NarcoticTask();

    void request(QTextStream &stream)
    {
        if (m_sleep) {
            stream << "SLEEP " << node()->id()
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
        }
        else {
            stream << "WAKE " << node()->id()
                << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
        }
    }

    void response(QTextStream &stream)
    {
        QString s = stream.readAll();
        if (s.startsWith("100")) {
            node()->goodQos();
            if (m_sleep)
                node()->sleep();
            else
                node()->wake();
        }
        else {
            node()->attention();
            node()->badQos();
        }
    }

protected:
    NarcoticTask(NodeItem *node, bool sleep):
        Task(node),

        m_sleep(sleep)
    {

    }

private:
    bool m_sleep;
};

Dialog::NarcoticTask::~NarcoticTask()
{

}

class Dialog::SleepTask:
    public NarcoticTask
{
public:
    SleepTask(NodeItem *node):
        NarcoticTask(node, true)
    {

    }

    ~SleepTask();
};

Dialog::SleepTask::~SleepTask()
{

}

class Dialog::WakeTask:
    public NarcoticTask
{
public:
    WakeTask(NodeItem *node):
        NarcoticTask(node, false)
    {

    }

    ~WakeTask();
};

Dialog::WakeTask::~WakeTask()
{

}


class Dialog::DimTask:
    public Task
{
public:
    DimTask(NodeItem *node):
        Task(node),

        m_dim(255)
    {

    }

    ~DimTask();

    void dim(int dim)
    {
        if (dim >= 0 && dim < 255)
            m_dim = dim;
    }

    void request(QTextStream &stream)
    {
        stream << "DIM " << node()->id() << " " << m_dim << " " << m_dim << " " << m_dim
               << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
    }

private:
    int m_dim;
};

Dialog::DimTask::~DimTask()
{

}


class Dialog::FrameTask:
    public Task
{
public:
    FrameTask(NodeItem *node):
        Task(node)
    {

    }

    ~FrameTask();

    void setFrame(const QByteArray &frame)
    {
        m_frame = frame;
    }

    void request(QTextStream &stream)
    {
        stream << "TPM2 " << node()->id() << QChar(QChar::LineFeed);
        stream << m_frame.toBase64();
        stream << QChar(QChar::LineFeed) << QChar(QChar::LineFeed);
    }

private:
    QByteArray m_frame;
};

Dialog::FrameTask::~FrameTask()
{

}



Dialog::Dialog(QWidget *parent):
    QDialog(parent)
{
    QSettings settings;
    setupUi(this);

    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning("DBus not available");
    }
    else if (!QDBusConnection::sessionBus().registerService("org.spl.nodes")) {
        qWarning("Cannot register DBus service: %s",
            qPrintable(QDBusConnection::sessionBus().lastError().message()));
    }
    else {
        QDBusConnection::sessionBus().registerObject("/", this, QDBusConnection::ExportScriptableSlots);
    }

    port = new QSerialPort(this);
    port->setBaudRate(QSerialPort::Baud57600);
    connect(
        port, SIGNAL(readyRead()),
        this, SLOT(readReady())
    );

    refreshPorts();
    portComboBox->setCurrentText(settings.value("port").toString());
    portComboBox->lineEdit()->addAction(refreshPortsAction, QLineEdit::TrailingPosition);
    connect(
        connectCheckBox, SIGNAL(toggled(bool)),
        this, SLOT(connectToggled(bool))
    );
    connect(
        refreshPortsAction, SIGNAL(triggered(bool)),
        this, SLOT(refreshPorts())
    );

    console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    idleTimer = new QTimer(this);
    idleTimer->setSingleShot(false);
    idleTimer->setInterval(100);
    idleIndex = 0;
    connect(
        idleTimer, SIGNAL(timeout()),
        this, SLOT(idle())
    );

    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(1000);
    currentTask = nullptr;
    connect(
        timeoutTimer, SIGNAL(timeout()),
        this, SLOT(timeout())
    );

    const int ids = settings.beginReadArray("nodes");
    for (int i = 0; i < ids; i++) {
        settings.setArrayIndex(i);
        const QVariant vid = settings.value("id", QVariant());
        if (vid.isNull())
            continue;

        addNode(vid.toInt());
    }
    settings.endArray();

    QMenu *addNodeMenu = new QMenu(this);
    addNodeMenu->addAction(addNodesAction);
    addNodeButton->setMenu(addNodeMenu);
    connect(
        addNodeButton, SIGNAL(clicked(bool)),
        this, SLOT(addNode())
    );
    connect(
        addNodesAction, SIGNAL(triggered(bool)),
        this, SLOT(addNodes())
    );
    connect(
        removeNodeButton, SIGNAL(clicked(bool)),
        this, SLOT(removeNode())
    );

    connect(
        wakeButton, SIGNAL(clicked(bool)),
        this, SLOT(wakeTask())
    );
    connect(
        sleepButton, SIGNAL(clicked(bool)),
        this, SLOT(sleepTask())
    );
    connect(
        sceneButton, SIGNAL(clicked(bool)),
        this, SLOT(startTask())
    );
    connect(
        pauseButton, SIGNAL(clicked(bool)),
        this, SLOT(pauseTask())
    );
    connect(
        stopButton, SIGNAL(clicked(bool)),
        this, SLOT(stopTask())
    );
    connect(
        skipButton, SIGNAL(clicked(bool)),
        this, SLOT(skipTask())
    );
    connect(
        dimButton, SIGNAL(clicked(bool)),
        this, SLOT(dimTask())
    );
    connect(
        frameButton, SIGNAL(clicked(bool)),
        this, SLOT(frameTask())
    );
    connect(
        matrixButton, SIGNAL(clicked(bool)),
        this, SLOT(matrixTask())
    );


    m_instance = this;
}

Dialog::~Dialog()
{
    m_instance = 0;

    port->close();
    qDeleteAll(tasks);
    tasks.clear();

    QSettings settings;
    settings.setValue("port", portComboBox->currentText());

    settings.beginWriteArray("nodes", treeWidget->topLevelItemCount());
    for (int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        NodeItem *item = static_cast<NodeItem *>(treeWidget->topLevelItem(i));
        settings.setArrayIndex(i);
        item->store(settings);
    }
    settings.endArray();
}

void Dialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;

    default:
        break;
    }
}

void Dialog::connectToggled(bool checked)
{
    const QString portName = portComboBox->currentText().trimmed();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "Schnittstelle öffnen",
            "Keine Schnittstelle eingegeben!");
        return;
    }

    if (checked) {
        port->setPortName(portName);
        if (port->open(QIODevice::ReadWrite)) {
            portComboBox->setEnabled(false);
            idleTimer->start();
        }
        else {
            connectCheckBox->setChecked(false);
            QMessageBox::warning(this, "Schnittstelle öffnen",
                QString("Schnittstelle kann nicht geöffnet werden: %1").arg(port->errorString()));
        }
    }
    else {
        idleTimer->stop();
        portComboBox->setEnabled(true);
        port->close();

        for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
            NodeItem *node = static_cast<NodeItem *>(item);
            node->setIcon(0, QIcon(":/offline.png"));
            node->wake();
        }
    }
}

void Dialog::refreshPorts()
{
    portComboBox->clear();
    for (const QSerialPortInfo &info: QSerialPortInfo::availablePorts())
        portComboBox->addItem(info.portName());
}

bool Dialog::addNode(int id)
{
    int i;
    for (i = 0; i < treeWidget->topLevelItemCount(); i++) {
        NodeItem *node = static_cast<NodeItem *>(treeWidget->topLevelItem(i));
        if (node->id() == id)
            return false;
        else if (node->id() > id)
            break;
    }

    NodeItem *node = new NodeItem(id);
    nodes.insert(id, node);
    treeWidget->insertTopLevelItem(i, node);
    treeWidget->setCurrentItem(node);
    return true;
}

void Dialog::addNode()
{
    bool ok;
    const int id = QInputDialog::getInt(this, "Empfänger hinzufügen", "Empfänger-ID:", 1, 0, 254, 1, &ok);
    if (!ok)
        return;

    if (!addNode(id)) {
        QMessageBox::information(this, "Empfänger hinzufügen",
            "Empfänger ist bereits in der Liste enthalten.");
    }
}

void Dialog::addNodes()
{
    bool ok;
    int first = QInputDialog::getInt(this, "Mehrere Empfänger hinzufügen", "Erste Empfänger-ID:", 1, 0, 254, 1, &ok);
    if (!ok)
        return;

    int count = QInputDialog::getInt(this, "Mehrere Empfänger hinzufügen", "Anzahl der Empfänger:", 1, 1, 254, 1, &ok);
    if (!ok)
        return;

    ok = true;
    while (count-- && first < 254)
        ok &= addNode(first++);

    if (!ok) {
        QMessageBox::information(this, "Mehrere Empfänger hinzufügen",
            "Einige Empfänger sind bereits in der Liste enthalten.");
    }
}

void Dialog::removeNode()
{
    QList<QTreeWidgetItem *> items = treeWidget->selectedItems();
    for (QLinkedList<Task *>::iterator it = tasks.begin(); it != tasks.end(); ) {
        if (items.contains((*it)->node())) {
            delete *it;
            it = tasks.erase(it);
        }
        else {
            it++;
        }
    }

    if (currentTask) {
        if (items.contains(currentTask->node())) {
            delete currentTask;
            currentTask = nullptr;
        }
    }

    for (const QTreeWidgetItem *item: qAsConst(items))
        nodes.remove( static_cast<const NodeItem *>(item)->id() );

    qDeleteAll(items);
    tasksProgressBar->setValue(tasks.count());
}

void Dialog::log(const QString &text, const QString prefix)
{
    const QStringList lines = text.split(QChar::LineFeed);
    console->appendPlainText(prefix + lines.join(QChar::LineFeed + prefix));
}

void Dialog::readReady()
{
    dataRead.append( QString::fromLatin1(port->readAll()) );
    if (dataRead.contains("\n\n")) {
        dataRead = dataRead.trimmed();
        log(dataRead, QStringLiteral("< "));

        QTextStream stream(&dataRead, QIODevice::ReadOnly);
        if (currentTask) {
            currentTask->response(stream);
            delete currentTask;
            currentTask = nullptr;
            timeoutTimer->stop();
        }
    }
}

void Dialog::timeout()
{
    if (currentTask) {
        log(dataRead, QStringLiteral("! Timeout"));
        currentTask->timeout();
        if (currentTask->ttl() > 0)
            postTask(currentTask);
        else
            delete currentTask;

        currentTask = nullptr;
    }
}

void Dialog::idle()
{
    if (!port->isOpen())
        return;

    if (treeWidget->topLevelItemCount() == 0)
        return;

    if (currentTask)
        return;

    if (tasks.isEmpty()) {
        if (idleIndex >= treeWidget->topLevelItemCount())
            idleIndex = 0;

        NodeItem *node = static_cast<NodeItem *>(treeWidget->topLevelItem(idleIndex++));
        postTask(new PingTask(node));
    }

    currentTask = popTask();
    const bool isWakeTask = dynamic_cast<WakeTask *>(currentTask);
    if (currentTask->node()->isAsleep() && !isWakeTask) {
        delete currentTask;
        currentTask = nullptr;
    }
    else {
        dataRead.clear();
        port->flush();

        QString s;
        QTextStream stream(&s, QIODevice::WriteOnly);
        currentTask->request(stream);

        if (isWakeTask)
            timeoutTimer->start(5000);
        else
            timeoutTimer->start(1000);
        port->write( s.toLatin1() );

        console->appendPlainText(QChar(QChar::LineFeed));
        log(s.trimmed(), "> ");
    }
}

void Dialog::postTask(Task *task)
{
    tasks.append(task);
    if (tasks.count() > tasksProgressBar->maximum())
        tasksProgressBar->setMaximum(tasks.count());
    tasksProgressBar->setValue(tasks.count());
}

Dialog::Task *Dialog::popTask()
{
    tasksProgressBar->setValue(tasks.count() - 1);
    return tasks.takeFirst();
}

void Dialog::sleepTask()
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems())
        postTask(new SleepTask( static_cast<NodeItem *>(item) ));
}

void Dialog::wakeTask()
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems())
        postTask(new WakeTask( static_cast<NodeItem *>(item) ));
}

void Dialog::startScene(int id, int scene)
{
    NodeItem *node = nodes.value(id, nullptr);
    if (!node)
        return;

    SceneTask *task = new SceneTask(node);
    task->setScene(scene);
    postTask(task);
}

void Dialog::startTask()
{
    bool ok;
    int scene = QInputDialog::getInt(this, "Szene aufrufen", "Szene:", 0, 0, 1000, 1, &ok);
    if (ok) {
        for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
            NodeItem *node = static_cast<NodeItem *>(item);
            SceneTask *task = new SceneTask(node);
            task->setScene(scene);
            postTask(task);
        }
    }
}

void Dialog::pauseTask()
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
        NodeItem *node = static_cast<NodeItem *>(item);
        SceneTask *task = new SceneTask(node);
        task->pause();
        postTask(task);
    }
}

void Dialog::stopScene(int id)
{
    NodeItem *node = nodes.value(id, nullptr);
    if (!node)
        return;

    SceneTask *task = new SceneTask(node);
    task->stop();
    postTask(task);
}

void Dialog::stopTask()
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
        NodeItem *node = static_cast<NodeItem *>(item);
        SceneTask *task = new SceneTask(node);
        task->stop();
        postTask(task);
    }
}

void Dialog::skipTask()
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
        NodeItem *node = static_cast<NodeItem *>(item);
        SceneTask *task = new SceneTask(node);
        task->skip();
        postTask(task);
    }
}

void Dialog::dimTask()
{
    bool ok;
    int percentage = QInputDialog::getInt(this, "Helligkeit einstellen", "Helligkeit:", 0, 0, 100, 1, &ok);
    if (ok) {
        for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
            NodeItem *node = static_cast<NodeItem *>(item);

            DimTask *dimTask = new DimTask(node);
            dimTask->dim(percentage * 255 / 100);
            postTask(dimTask);
        }
    }
}

void Dialog::frameTask()
{
    Frame *frameDialog = new Frame(this);
    frameDialog->show();
}

void Dialog::matrixTask()
{
    Matrix *matrixDialog = new Matrix(this);
    matrixDialog->setAttribute(Qt::WA_DeleteOnClose);
    matrixDialog->show();
}

void Dialog::sendFrame(const QByteArray &frame)
{
    for (QTreeWidgetItem *item: treeWidget->selectedItems()) {
        NodeItem *node = static_cast<NodeItem *>(item);
        FrameTask *task = new FrameTask(node);
        postTask(task);

        const int chunk = 2048;
        for (int i = 0; i < frame.count(); i += chunk) {
            task = new FrameTask(node);
            task->setFrame(frame.mid(i, chunk));
            postTask(task);
        }
    }
}

QAbstractItemModel *Dialog::model() const
{
    return treeWidget->model();
}
