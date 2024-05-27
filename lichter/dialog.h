
#ifndef DIALOG_H
#define DIALOG_H

#include <QtCore/QVector>
#include <QtGui/QColor>

#include "ui_dialog.h"

class QSerialPort;
class QDataStream;
class QSettings;
class QFile;
class QTimer;
class QColor;

class Dialog;

class AbstractMode
{
public:
    AbstractMode(Dialog *dialog);
    virtual ~AbstractMode();

    virtual void prepare(QVector<QColor> &/*leds*/) { }
    virtual void frame(QVector<QColor> &/*leds*/) { }
    virtual void scene(QVector<QColor> &/*leds*/) { }
    virtual void paletteClicked(QVector<QColor> &/*leds*/, const QColor &/*color*/) { }

protected:
    void cycle();

    Dialog *m_dialog;
};


class DialogSaver:
    public QObject
{
    Q_OBJECT

public:
    DialogSaver(QWidget *widget);

    void save(QSettings &settings);
    void load(QSettings &settings);

private:
    void saveWidgets(QSettings &settings, const QWidget *widget);
    void saveWidget(QSettings &settings, const QWidget *widget);
    void loadWidgets(QSettings &settings, QWidget *widget);
    void loadWidget(QSettings &settings, QWidget *widget);
};


class Dialog:
    public QDialog,
    private Ui::Dialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();

protected:
    void changeEvent(QEvent *e);

private:
    QSerialPort *port;
    QFile *file;
    QTimer *frameTimer;
    QTimer *sceneTimer;
    QTimer *dimTimer;
    QVector<QColor> leds;
    QVector<QColor> segment;

    class SingleColorMode;
    class PulseMode;
    class ShootingStarMode;
    class SnakeMode;
    class SparklingMode;
    class FireMode;
    QVector<AbstractMode *> modes;
    AbstractMode *currentMode;

    friend class AbstractMode;

private slots:
    void connectToggled(bool checked);
    void fileToggled(bool checked);
    void browseFile();
    void startToggled(bool checked);
    void singleShotToggled(bool checked);
    void ledsChanged(int value);
    void segmentChanged(int value);
    void positionChanged(int value);
    void dimToggled(bool checked);
    void emitFrame();
    void emitFrame(QDataStream &stream);
    void scene();

    void loadSettings(QAction *action);
    void saveSettings(QAction *action);
    void saveSettings();

    void modeChanged(int index);

    void changeRGB();
    void changeHSV();

    void addPalette(const QColor &color, const QString &name = QString());
    void paletteAddClicked();
    void paletteRemoveClicked();
    void singleColorToPaletteClicked();
    void paletteDoubleClicked(QListWidgetItem *item);
};

#endif // DIALOG_H
