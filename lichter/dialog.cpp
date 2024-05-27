
#include <QtDebug>

#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QColorDialog>
#include <QtGui/QPixmap>

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

#include "dialog.h"

AbstractMode::AbstractMode(Dialog *dialog):
    m_dialog(dialog)
{

}

AbstractMode::~AbstractMode()
{

}

void AbstractMode::cycle()
{
    if (m_dialog->singleShotButton->isChecked()) {
        m_dialog->startButton->setChecked(false);
        m_dialog->singleShotButton->setChecked(false);
        m_dialog->singleShotButton->setEnabled(true);
        m_dialog->startButton->setEnabled(true);
    }
}


DialogSaver::DialogSaver(QWidget *widget):
    QObject(widget)
{

}

void DialogSaver::save(QSettings &settings)
{
    const QWidget *widget = qobject_cast<const QWidget *>(parent());
    if (!widget)
        return;

    saveWidget(settings, widget);
}

void DialogSaver::load(QSettings &settings)
{
    QWidget *widget = qobject_cast<QWidget *>(parent());
    if (!widget)
        return;

    loadWidget(settings, widget);
}

void DialogSaver::saveWidgets(QSettings &settings, const QWidget *widget)
{
    const QObjectList objects = widget->children();
    if (objects.isEmpty())
        return;

    settings.beginGroup(widget->objectName());
    for (const QObject *object: objects) {
        if (!object->isWidgetType())
            continue;

        saveWidget(settings, qobject_cast<const QWidget *>(object));
    }
    settings.endGroup();
}

void DialogSaver::saveWidget(QSettings &settings, const QWidget *widget)
{
    const QString name = widget->objectName();
    if (const QLineEdit *edit = qobject_cast<const QLineEdit *>(widget)) {
        settings.setValue(name, edit->text());
    }
    else if (const QCheckBox *check = qobject_cast<const QCheckBox *>(widget)) {
        settings.setValue(name, check->isChecked());
    }
    else if (const QSlider *slider = qobject_cast<const QSlider *>(widget)) {
        settings.setValue(name, slider->value());
    }
    else {
        saveWidgets(settings, widget);
    }
}

void DialogSaver::loadWidget(QSettings &settings, QWidget *widget)
{
    const QString name = widget->objectName();
    if (QLineEdit *edit = qobject_cast<QLineEdit *>(widget)) {
        edit->setText(settings.value(name).toString());
    }
    else if (QCheckBox *check = qobject_cast<QCheckBox *>(widget)) {
        check->setChecked(settings.value(name).toBool());
    }
    else if (QSlider *slider = qobject_cast<QSlider *>(widget)) {
        slider->setValue(settings.value(name).toInt());
    }
    else {
        loadWidgets(settings, widget);
    }
}


void DialogSaver::loadWidgets(QSettings &settings, QWidget *widget)
{
    const QObjectList objects = widget->children();
    if (objects.isEmpty())
        return;

    settings.beginGroup(widget->objectName());
    for (QObject *object: objects) {
        if (!object->isWidgetType())
            continue;

        loadWidget(settings, qobject_cast<QWidget *>(object));
    }
    settings.endGroup();
}




class Dialog::SingleColorMode:
    public AbstractMode
{
public:
    SingleColorMode(Dialog *dialog):
        AbstractMode(dialog)
    {

    }

    void frame(QVector<QColor> &leds)
    {
        leds.fill(QColor::fromRgb(
              m_dialog->redSlider->value(),
              m_dialog->greenSlider->value(),
              m_dialog->blueSlider->value()));

        cycle();
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        m_dialog->redSlider->setValue(color.red());
        m_dialog->greenSlider->setValue(color.green());
        m_dialog->blueSlider->setValue(color.blue());
    }
};

class Dialog::PulseMode:
    public AbstractMode
{
public:
    PulseMode(Dialog *dialog):
        AbstractMode(dialog)
    {

    }

    void prepare(QVector<QColor> &)
    {
        state = Reset;
    }

    void scene(QVector<QColor> &leds)
    {
        const int step = 1;

        switch (state) {
        case Increment:
            intensity += step;
            if (intensity >= 1000) {
                intensity = 1000;
                state = Decrement;
            }

            break;

        case Decrement:
            intensity -= step;
            if (intensity <= 0) {
                intensity = 0;
                pause = 0;
                state = Pause;
            }

            break;

        case Pause:
            pause++;
            m_dialog->pulsePauseProgressBar->setRange(0, m_dialog->pulsePauseSlider->value());
            m_dialog->pulsePauseProgressBar->setValue(pause);
            if (pause >= m_dialog->pulsePauseSlider->value()) {
                m_dialog->pulsePauseProgressBar->setValue(0);
                state = Reset;
                cycle();
            }

            break;

        default:
            intensity = 0;
            state = Increment;
            break;
        }

        m_dialog->pulseIntensityProgressBar->setValue(intensity);
        leds.fill(QColor::fromRgb(
            m_color.red() * intensity / 1000,
            m_color.green() * intensity / 1000,
            m_color.blue() * intensity / 1000));
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        m_color = color;
    }

private:
    QColor m_color;
    int intensity;
    int pause;
    enum State {
        Reset,
        Increment,
        Decrement,
        Pause,
    } state;
};

class Dialog::ShootingStarMode:
    public AbstractMode
{
public:
    ShootingStarMode(Dialog *dialog):
        AbstractMode(dialog)
    {

    }

    void prepare(QVector<QColor> &)
    {
        state = Reset;
    }

    void scene(QVector<QColor> &leds)
    {
        const int length = leds.count() * m_dialog->shootingStarLengthSlider->value() / m_dialog->shootingStarLengthSlider->maximum();
        const int fading = length * m_dialog->shootingStarFadingSlider->value() / m_dialog->shootingStarFadingSlider->maximum() / 2;
        const int sparklingLength = length * m_dialog->shootingStarSparklingLengthSlider->value() / m_dialog->shootingStarSparklingLengthSlider->maximum();
        const int sparklingCount = sparklingLength * m_dialog->shootingStarSparklingCountSlider->value() / m_dialog->shootingStarSparklingCountSlider->maximum();

        switch (state) {
        case Star:
            position++;
            if (position > leds.count()) {
                pause = 0;
                state = Pause;
            }
            else {
                leds.fill(Qt::black);
                for (int i = qMax(position, 0); i < qMin(position + length, leds.count()); i++) {
                    const int index = i - position;
                    double rate = 1.0;
                    if (index < fading)
                        rate = (double) index / fading;
                    else if ( (index > length - fading) && !sparklingLength )
                        rate = (double) (fading - (index - (length - fading))) / fading;

                    leds[i] = QColor::fromRgb(
                        m_color.red() * rate,
                        m_color.green() * rate,
                        m_color.blue() * rate);
                }

                if (sparklingLength) {
                    const int head = position + length;
                    const int tail = qMax(head - sparklingLength, 0);
                    if (tail < leds.count()) {
                        for (int i = 0; i < sparklingCount / 3; i++) {
                            const int n = rnd(tail, qMin(head, leds.count() - 1));
                            leds[n] = Qt::white;
                        }
                    }
                }
            }

            break;

        case Pause:
            pause++;
            m_dialog->shootingStarPauseProgressBar->setRange(0, m_dialog->shootingStarPauseSlider->value());
            m_dialog->shootingStarPauseProgressBar->setValue(pause);
            if (pause >= m_dialog->shootingStarPauseSlider->value()) {
                m_dialog->shootingStarPauseProgressBar->setValue(0);
                state = Reset;
                cycle();
            }

            break;

        default:
            position = -length;
            state = Star;
            break;
        }
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        m_color = color;
    }

private:
    QColor m_color;
    int pause;
    int position;
    enum State {
        Reset,
        Star,
        Pause,
    } state;

    int rnd(int low, int high) const
    {
        return qrand() % ((high + 1) - low) + low;
    }
};


class Dialog::SnakeMode:
    public AbstractMode
{
public:
    SnakeMode(Dialog *dialog):
        AbstractMode(dialog)
    {

    }

    void scene(QVector<QColor> &leds)
    {
        const int length = leds.count() * m_dialog->snakeLengthSlider->value() / m_dialog->snakeLengthSlider->maximum();
        const int fading = length * m_dialog->snakeFadingSlider->value() / m_dialog->snakeFadingSlider->maximum() / 2;

        /*position++;
        if (position >= leds.count())
            position = 0;*/

        position--;
            if (position < 0)
                position =  leds.count()-1;

        leds.fill(Qt::black);
        for (int i = position; i < position + length; i++) {
            const int index = i - position;
            double rate = 1.0;
            if (index < fading)
                rate = (double) index / fading;
            else if (index > length - fading)
                rate = (double) (fading - (index - (length - fading))) / fading;

            leds[i % leds.count()] = QColor::fromRgb(
                m_color.red() * rate,
                m_color.green() * rate,
                m_color.blue() * rate);
        }
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        m_color = color;
    }

private:
    QColor m_color;
    int position;
};


class Dialog::SparklingMode:
    public AbstractMode
{
public:
    SparklingMode(Dialog *dialog):
        AbstractMode(dialog)
    {

    }

    void prepare(QVector<QColor> &leds)
    {
        leds.fill(QColor::fromRgb(0, 0, 0, 0));
    }

    void frame(QVector<QColor> &leds)
    {
        const int duration = m_dialog->sparklingDurationSlider->value();
        for (QColor &led: leds) {
            if (led.alpha()) {
                if (m_dialog->sparklingFadeCheckBox->isChecked()) {
                    const double rate = qMin(1.0, (double) led.alpha() / duration);
                    led = QColor::fromRgb(
                        led.red() * rate,
                        led.green() * rate,
                        led.blue() * rate,
                        led.alpha() - 1);
                }
                else {
                    led.setAlpha(led.alpha() - 1);
                }
            }
            else {
                led = Qt::black;
            }
        }
    }

    void scene(QVector<QColor> &leds)
    {
        const int sparklingCount = leds.count() * m_dialog->sparklingCountSlider->value() / m_dialog->sparklingCountSlider->maximum();
        const int duration = m_dialog->sparklingDurationSlider->value();
        QColor color = m_color;
        m_color.setAlpha(duration);
        for (int i = 0; i < sparklingCount / 3; i++) {
            const int n = rnd(0, leds.count() - 1);
            leds[n] = color;
        }
        cycle();
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        m_color = color;
    }

private:
    QColor m_color;
    int timeout;

    int rnd(int low, int high) const
    {
        return qrand() % ((high + 1) - low) + low;
    }
};


class Dialog::FireMode:
    public AbstractMode
{
public:
    FireMode(Dialog *dialog):
        AbstractMode(dialog),

        rows(m_dialog->fireIntensitySlider->maximum() + 1)
    {
        palette.fill(Qt::black, 256);
    }

    void scene(QVector<QColor> &leds)
    {
        if (heats.count() != leds.count())
            heats.fill( QVector<int>(rows, 0), leds.count() );

        const int cooling = m_dialog->fireCoolingSlider->value();
        const int sparking = 120;
        for (QVector<int> &heat: heats) {
            for (int y = 0; y < rows; y++)
                heat[y] = qMax(heat[y] - rnd(0, cooling), 0);

            for (int y = rows - 1; y >= 2; y--)
                heat[y] = (heat[y - 1] + 2 * heat[y - 2]) / 3;

            if (rnd(0, 255) < sparking) {
                const int y = rnd(0, 8);
                heat[y] = qMin(heat[y] + rnd(100, 200) * (8 - y) / 8, 255);
            }
        }

        const int line = rows - 1 - m_dialog->fireIntensitySlider->value();
        for (int i = 0; i < leds.count(); i++)
            leds[i] = palette[qMin(heats[i][line], 255)];

        cycle();
    }

    void paletteClicked(QVector<QColor> &/*leds*/, const QColor &color)
    {
        const int hue = color.hue();
        int i = 0;
        for (i = 0; i < palette.count() * 3 / 4; i++)
            palette[i] = QColor::fromHsv(hue, 255, i * 4 / 3);

        for (; i < palette.count(); i++)
            palette[i] = QColor::fromHsv(hue, 255 - i, 255);
    }

private:
    const int rows;
    QVector< QVector<int> > heats;

    QVector<QColor> palette;

    int rnd(int low, int high) const
    {
        return qrand() % ((high + 1) - low) + low;
    }
};



Dialog::Dialog(QWidget *parent) :
    QDialog(parent)
{
    QSettings settings;
    setupUi(this);

    port = new QSerialPort(this);
    port->setBaudRate(500000);
    port->setStopBits(QSerialPort::TwoStop);
    //port->setBaudRate(QSerialPort::Baud115200);
    //port->setBaudRate(QSerialPort::Baud19200);
    //port->setBaudRate(QSerialPort::Baud9600);
    file = new QFile(this);

    for (const QSerialPortInfo &info: QSerialPortInfo::availablePorts())
        portComboBox->addItem(info.portName());

    portComboBox->setCurrentText(settings.value("port").toString());

    connect(
        connectCheckBox, SIGNAL(toggled(bool)),
        this, SLOT(connectToggled(bool))
    );
    connect(
        fileCheckBox, SIGNAL(toggled(bool)),
        this, SLOT(fileToggled(bool))
    );
    connect(
        browseFileButton, SIGNAL(clicked(bool)),
        this, SLOT(browseFile())
    );

    connect(
        ledsSpinBox, SIGNAL(valueChanged(int)),
        this, SLOT(ledsChanged(int))
    );
    ledsSpinBox->setValue(settings.value("leds").toInt());
    ledsChanged(ledsSpinBox->value());

    connect(
        segmentSlider, SIGNAL(valueChanged(int)),
        this, SLOT(segmentChanged(int))
    );
    connect(
        positionSlider, SIGNAL(valueChanged(int)),
        this, SLOT(positionChanged(int))
    );

    modes.append(new SingleColorMode(this));
    modes.append(new PulseMode(this));
    modes.append(new ShootingStarMode(this));
    modes.append(new SnakeMode(this));
    modes.append(new SparklingMode(this));
    modes.append(new FireMode(this));
    connect(
        tabWidget, SIGNAL(currentChanged(int)),
        this, SLOT(modeChanged(int))
    );
    currentMode = 0;
    modeChanged(0);

    connect(redSlider, SIGNAL(valueChanged(int)), this, SLOT(changeRGB()));
    connect(greenSlider, SIGNAL(valueChanged(int)), this, SLOT(changeRGB()));
    connect(blueSlider, SIGNAL(valueChanged(int)), this, SLOT(changeRGB()));
    connect(hueSlider, SIGNAL(valueChanged(int)), this, SLOT(changeHSV()));
    connect(satSlider, SIGNAL(valueChanged(int)), this, SLOT(changeHSV()));
    connect(valueSlider, SIGNAL(valueChanged(int)), this, SLOT(changeHSV()));

    int count = settings.beginReadArray("palette");
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        const QColor color = settings.value("color").value<QColor>();
        const QString name = settings.value("name").toString();
        addPalette(color, name);
    }
    settings.endArray();

    connect(
        paletteAddButton, SIGNAL(clicked()),
        this, SLOT(paletteAddClicked())
    );
    connect(
        paletteRemoveButton, SIGNAL(clicked()),
        this, SLOT(paletteRemoveClicked())
    );
    connect(
        singleColorToPaletteButton, SIGNAL(clicked()),
        this, SLOT(singleColorToPaletteClicked())
    );
    connect(
        paletteList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
        this, SLOT(paletteDoubleClicked(QListWidgetItem*))
    );

    loadSettingsButton->setMenu(new QMenu(this));
    saveSettingsButton->setMenu(new QMenu(this));
    count = settings.beginReadArray("settings");
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        const QString name = settings.value("name").toString();
        loadSettingsButton->menu()->addAction(name);
        saveSettingsButton->menu()->addAction(name);
    }
    settings.endArray();

    connect(
        saveSettingsButton, SIGNAL(clicked(bool)),
        this, SLOT(saveSettings())
    );
    connect(
        saveSettingsButton->menu(), SIGNAL(triggered(QAction*)),
        this, SLOT(saveSettings(QAction*))
    );
    connect(
        loadSettingsButton->menu(), SIGNAL(triggered(QAction*)),
        this, SLOT(loadSettings(QAction*))
    );

    frameTimer = new QTimer(this);
    frameTimer->setInterval(50);
    frameTimer->setSingleShot(false);
    connect(
        frameTimer, SIGNAL(timeout()),
        this, SLOT(emitFrame())
    );

    sceneTimer = new QTimer(this);
    sceneTimer->setSingleShot(false);
    connect(
        sceneTimer, SIGNAL(timeout()),
        this, SLOT(scene())
    );
    connect(
        speedSlider, SIGNAL(valueChanged(int)),
        sceneTimer, SLOT(start(int))
    );
    sceneTimer->setInterval(speedSlider->value());

    connect(
        startButton, SIGNAL(toggled(bool)),
        this, SLOT(startToggled(bool))
    );
    connect(
        singleShotButton, SIGNAL(toggled(bool)),
        this, SLOT(singleShotToggled(bool))
    );


    dimTimer = new QTimer(this);
    dimTimer->setSingleShot(false);
    dimTimer->setInterval(10);
    connect(
        dimUpButton, SIGNAL(toggled(bool)),
        this, SLOT(dimToggled(bool))
    );
    connect(
        dimDownButton, SIGNAL(toggled(bool)),
        this, SLOT(dimToggled(bool))
    );
    connect(
        dimTimer, &QTimer::timeout,
        [=] {
            if (dimUpButton->isChecked()) {
                masterSlider->setValue(masterSlider->value() + 5);
                if (masterSlider->value() == masterSlider->maximum())
                    dimUpButton->setChecked(false);
            }
            else {
                masterSlider->setValue(masterSlider->value() - 5);
                if (masterSlider->value() == masterSlider->minimum())
                    dimDownButton->setChecked(false);
            }
        });
}

Dialog::~Dialog()
{
    frameTimer->stop();
    sceneTimer->stop();
    port->close();
    file->close();

    QSettings settings;
    settings.setValue("port", portComboBox->currentText());
    settings.setValue("leds", ledsSpinBox->value());

    settings.beginWriteArray("palette");
    for (int i = 0; i < paletteList->count(); i++) {
        const QListWidgetItem *item = paletteList->item(i);
        settings.setArrayIndex(i);
        settings.setValue("color", item->data(Qt::UserRole).value<QColor>());
        settings.setValue("name", item->data(Qt::DisplayRole).toString());
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
        if (port->open(QIODevice::WriteOnly)) {
            portComboBox->setEnabled(false);
            leds.resize(ledsSpinBox->value());
            segment.resize(leds.count());
        }
        else {
            connectCheckBox->setChecked(false);
            QMessageBox::warning(this, "Schnittstelle öffnen",
                QString("Schnittstelle kann nicht geöffnet werden: %1").arg(port->errorString()));
        }
    }
    else {
        portComboBox->setEnabled(true);
        port->close();
    }
}

void Dialog::fileToggled(bool checked)
{
    const QString fileName = fileEdit->text().trimmed();
    if (fileName.isEmpty()) {
        QMessageBox::warning(this, "Datei öffnen",
            "Keine Datei eingegeben!");
        return;
    }

    if (checked) {
        file->setFileName(fileName);
        if (file->open(QIODevice::Append)) {
            fileEdit->setEnabled(false);
        }
        else {
            fileCheckBox->setChecked(false);
            QMessageBox::warning(this, "Datei öffnen",
                QString("Datei kann nicht geöffnet werden: %1").arg(file->errorString()));
        }
    }
    else {
        fileEdit->setEnabled(true);
        file->close();
    }
}

void Dialog::browseFile()
{
    const QString file = QFileDialog::getSaveFileName(this, "Ausgabedatei wählen");
    if (!file.isEmpty())
        fileEdit->setText(file);
}

void Dialog::startToggled(bool checked)
{
    if (checked) {
        if (currentMode)
            currentMode->prepare(segment);

        frameTimer->start();
        sceneTimer->start();
    }
    else {
        frameTimer->stop();
        sceneTimer->stop();
    }
}

void Dialog::singleShotToggled(bool checked)
{
    if (checked) {
        singleShotButton->setEnabled(false);
        startButton->setChecked(false);
        startButton->setEnabled(false);
        startButton->setChecked(true);
    }
}

void Dialog::ledsChanged(int value)
{
    leds.resize(value);
    segmentSlider->setRange(1, leds.count());
    segmentSlider->setValue(leds.count());
    positionSlider->setRange(0, leds.count() - segmentSlider->value());
}

void Dialog::segmentChanged(int value)
{
    segment.resize(value);
    positionSlider->setRange(0, leds.count() - segmentSlider->value());
}

void Dialog::positionChanged(int /*value*/)
{

}

void Dialog::dimToggled(bool checked)
{
    if (!checked) {
        dimTimer->stop();
        return;
    }

    if (sender() == dimUpButton)
        dimDownButton->setChecked(false);
    else
        dimUpButton->setChecked(false);
    dimTimer->start();
}

void Dialog::modeChanged(int index)
{
    if (index >= 0 && index < modes.count()) {
        currentMode = modes.at(index);
        currentMode->prepare(segment);
    }
}

void Dialog::changeRGB()
{
    const QColor color = QColor::fromRgb(
        redSlider->value(),
        greenSlider->value(),
        blueSlider->value());

    QSignalBlocker hueBlocker(hueSlider);
    QSignalBlocker satBlocker(satSlider);
    QSignalBlocker valueBlocker(valueSlider);
    hueSlider->setValue(color.hue());
    satSlider->setValue(color.saturation());
    valueSlider->setValue(color.value());
}

void Dialog::changeHSV()
{
    const QColor color = QColor::fromHsv(
        hueSlider->value(),
        satSlider->value(),
        valueSlider->value());

    QSignalBlocker redBlocker(redSlider);
    QSignalBlocker greenBlocker(greenSlider);
    QSignalBlocker blueBlocker(blueSlider);
    redSlider->setValue(color.red());
    greenSlider->setValue(color.green());
    blueSlider->setValue(color.blue());
}

void Dialog::addPalette(const QColor &color, const QString &name)
{
    QListWidgetItem *item = new QListWidgetItem(name.isNull() ? "Neue Farbe" : name);
    item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    QPixmap pixmap(32, 32);
    pixmap.fill(color);
    item->setIcon(pixmap);
    item->setData(Qt::UserRole, color);
    paletteList->addItem(item);
    paletteList->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
}

void Dialog::paletteAddClicked()
{
    QColor currentColor;
    if (paletteList->currentItem())
        currentColor = paletteList->currentItem()->backgroundColor();

    const QColor color = QColorDialog::getColor(currentColor, this, "Farbe wählen", QColorDialog::DontUseNativeDialog);
    if (color.isValid())
        addPalette(color);
}

void Dialog::paletteRemoveClicked()
{
    QList<QListWidgetItem *> items = paletteList->selectedItems();
    qDeleteAll(items);
}

void Dialog::singleColorToPaletteClicked()
{
    addPalette(QColor::fromRgb(
        redSlider->value(),
        greenSlider->value(),
        blueSlider->value()));
}

void Dialog::paletteDoubleClicked(QListWidgetItem *item)
{
    if (currentMode)
        currentMode->paletteClicked( segment, item->data(Qt::UserRole).value<QColor>() );
}


void Dialog::saveSettings()
{
    const QString name = QInputDialog::getText(this, "Szene speichern", "Name:");
    if (name.isEmpty())
        return;

    loadSettingsButton->menu()->addAction(name);
    QAction *action = saveSettingsButton->menu()->addAction(name);
    action->trigger();
}

void Dialog::saveSettings(QAction *action)
{
    const QString name = action->text();
    saveSettingsButton->setText(QString("Speichern [%1]").arg(name));

    QSettings settings;
    int index;
    const int count = settings.beginReadArray("settings");
    for (index = 0; index < count; index++) {
        settings.setArrayIndex(index);
        if (settings.value("name").toString() == name)
            break;
    }
    settings.endArray();

    settings.beginWriteArray("settings");
    settings.setArrayIndex(index);
    settings.setValue("name", name);

    DialogSaver saver(tabWidget);
    saver.save(settings);

    settings.endArray();
}

void Dialog::loadSettings(QAction *action)
{
    const QString name = action->text();
    loadSettingsButton->setText(QString("Laden [%1]").arg(name));
    loadSettingsButton->setDefaultAction(action);

    QSettings settings;
    int index;
    const int count = settings.beginReadArray("settings");
    for (index = 0; index < count; index++) {
        settings.setArrayIndex(index);
        if (settings.value("name").toString() == name) {
            DialogSaver loader(tabWidget);
            loader.load(settings);
            break;
        }
    }
    settings.endArray();
}

QByteArray compr(const QByteArray &a)
{
    QByteArray compressed;
    QDataStream s(&compressed, QIODevice::WriteOnly);

    int i = 0;
    while (i < a.count()) {
        s << (uint8_t) a.at(i++);
        if (i < 6)
            continue;

        if (a.mid(i - 6, 3) == a.mid(i - 3, 3)) {
            uint8_t n = 0;
            int j = i + 3;
            while (j < a.length()) {
                if (a.mid(i - 3, 3) != a.mid(j - 3, 3))
                    break;

                j += 3;
                n++;
                if (n == 250)
                    break;
            }

            s << n;
            i = j - 3;
        }

    }

    return compressed;
}

QColor discretize(const QColor &color, qreal threshold)
{
    const qreal min = qMin(color.redF(), qMin(color.greenF(), color.blueF()));
    if (min > threshold / 10.0 && min < threshold) {
        const qreal fac = threshold / min;
        return QColor::fromRgbF(
            color.redF() * fac,
            color.greenF() * fac,
            color.blueF() * fac);
    }

    return color;
}

void Dialog::emitFrame(QDataStream &stream)
{
    QByteArray a;
    {
        QDataStream s(&a, QIODevice::WriteOnly);
        const int master = masterSlider->value();
        for (const QColor &led: leds) {
            QColor c = QColor::fromRgbF(
                led.redF() * master / masterSlider->maximum(),
                led.greenF() * master / masterSlider->maximum(),
                led.blueF() * master / masterSlider->maximum());

            if (discretizeSlider->value())
                c = discretize(c, discretizeSlider->value() / 100.0);

            s << quint8(c.red());
            s << quint8(c.green());
            s << quint8(c.blue());
        }
    }

    QByteArray c;
    if (compressCheckBox->isChecked()) {
        c = compr(a);
        stream << quint8(0xC9) << quint8(0xCA);
    }
    else {
        c = a;
        stream << quint8(0xC9) << quint8(0xDA);
    }

    const int length = c.length();
    stream << quint8((length >> 8) & 0xFF);
    stream << quint8(length & 0xFF);
    stream.writeRawData(c.constData(), c.count());
    stream << quint8(0x36);
}

void Dialog::emitFrame()
{
    /*if (!leds.isEmpty()) {
        const int master = masterSlider->value();
        qDebug()
            << quint8(leds.first().red() * master / masterSlider->maximum())
            << quint8(leds.first().green() * master / masterSlider->maximum())
            << quint8(leds.first().blue() * master / masterSlider->maximum());
    }*/


    if (currentMode)
        currentMode->frame(segment);

    leds.fill(Qt::black);
    const bool reverse = reverseCheckBox->isChecked();
    for (int i = positionSlider->value(); i < positionSlider->value() + segmentSlider->value(); i++) {
        if (reverse)
            leds[leds.count() - i - 1] = segment[i - positionSlider->value()];
        else
            leds[i] = segment[i - positionSlider->value()];
    }

    if (port->isOpen()) {
        if (port->bytesToWrite() == 0) {
            QDataStream stream(port);
            emitFrame(stream);
        }
    }

    if (file->isOpen()) {
        QDataStream stream(file);
        emitFrame(stream);
    }
}

void Dialog::scene()
{
    if (currentMode)
        currentMode->scene(segment);
}
