#include "snigdhaosblackbox.h"  // Includes the header file for the SnigdhaOSBlackbox class to use its declarations and functionality.
#include "./ui_snigdhaosblackbox.h"  // Includes the auto-generated header file for the UI created using Qt Designer.

#include <QCheckBox>  // Used to manage checkbox UI components.
#include <QDebug>  // Provides tools for debugging, logging information, and printing messages to the console.
#include <QFileInfo>  // Allows access to file metadata, such as checking file modification times.
#include <QProcess>  // Used to manage and interact with external processes (such as running commands in the terminal).
#include <QScrollArea>  // Provides a scrollable area in the UI to allow navigation through large widgets.
#include <QTemporaryFile>  // Creates temporary files that are automatically deleted after use.
#include <QTimer>  // Provides functionality for scheduling tasks with delays or intervals.
#include <QtNetwork/QNetworkReply>  // Handles responses from network requests (used to check internet connectivity).
#include <unistd.h>  // Provides POSIX functions, used here for process management (e.g., restarting the application).

const char* INTERNET_CHECK_URL = "https://snigdha-os.github.io/";  // URL used to verify internet connectivity by sending a network request.

SnigdhaOSBlackbox::SnigdhaOSBlackbox(QWidget *parent, QString state)
    : QMainWindow(parent)  // Calls the constructor of the QMainWindow base class to initialize the main window with the parent widget.
    , ui(new Ui::SnigdhaOSBlackbox)  // Initializes the user interface (UI) for the SnigdhaOSBlackbox window, using the UI class auto-generated by Qt Designer.
{
    // Sets the window icon to the specified file path, ensuring that the application window will display the given icon (SVG format).
    this->setWindowIcon(QIcon("/usr/share/pixmaps/snigdhaos-blackbox.svg"));

    // Initializes the user interface, setting up the UI components (buttons, labels, etc.) in the SnigdhaOSBlackbox window.
    ui->setupUi(this);

    // Modifies the window flags to disable the close button on the window (i.e., the application cannot be closed directly via the window).
    this->setWindowFlags(this->windowFlags() & -Qt::WindowCloseButtonHint);

    // Gets the last modified timestamp of the executable file (the current running application), which is useful for checking when the application was last updated.
    executable_modify_date = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();

    // Updates the application state based on the provided `state` parameter.
    // This can be a state like "WELCOME", "INTERNET", etc., depending on the condition provided by the caller.
    updateState(state);
}

SnigdhaOSBlackbox::~SnigdhaOSBlackbox()
{
    // Frees the memory allocated for the user interface (UI) object.
    // The 'ui' pointer was allocated in the constructor, and it's responsible for managing the UI components of the SnigdhaOSBlackbox window.
    delete ui;
}

void SnigdhaOSBlackbox::doInternetUpRequest() {
    // Create a new QNetworkAccessManager instance for managing the network request.
    // The 'this' pointer ensures the manager is associated with the current object (SnigdhaOSBlackbox).
    QNetworkAccessManager* network_manager = new QNetworkAccessManager(this);

    // Send a HEAD request to the specified URL to check if the internet connection is up.
    // The HEAD request is used to check the server status without downloading the full content.
    QNetworkReply* network_reply = network_manager->head(QNetworkRequest(QString(INTERNET_CHECK_URL)));

    // Create a new QTimer instance that will be used to implement a timeout for the network request.
    QTimer* timer = new QTimer(this);
    
    // Set the timer to single-shot mode, so it will only trigger once after the specified interval (5 seconds).
    timer->setSingleShot(true);

    // Start the timer with a 5-second interval (5000 milliseconds).
    timer->start(5000); // 5 sec

    // Connect the timer's timeout signal to a lambda function.
    // This function will execute if the timer expires (i.e., after 5 seconds), aborting the network request.
    connect(timer, &QTimer::timeout, this, [this, timer, network_reply, network_manager]() {
        // Clean up the resources associated with the timer, network reply, and network manager.
        // Deleting them to prevent memory leaks.
        timer->deleteLater();
        network_reply->abort();  // Abort the network request as the timeout occurred.
        network_reply->deleteLater();  // Delete the network reply object to free resources.
        network_manager->deleteLater();  // Delete the network manager object to free resources.

        // Retry the internet check by calling this function recursively.
        doInternetUpRequest();
    });

    // Connect the network reply's finished signal to a lambda function that handles the network reply.
    connect(network_reply, &QNetworkReply::finished, this, [this, timer, network_reply, network_manager]() {
        // Stop the timer once the network reply is finished, as we no longer need to wait for the timeout.
        timer->stop();

        // Clean up the resources associated with the timer, network reply, and network manager after the request finishes.
        timer->deleteLater();
        network_reply->deleteLater();
        network_manager->deleteLater();

        // Check if there was no error with the network reply (i.e., the internet is up).
        if (network_reply->error() == network_reply->NoError) {
            // If there was no error, update the state to indicate that the update process can begin.
            updateState(State::UPDATE);
        } else {
            // If there was an error, retry the internet check by calling this function recursively.
            doInternetUpRequest();
        }
    });
}

void SnigdhaOSBlackbox::doUpdate() {
    // Check if the environment variable "SNIGDHAOS_BLACKBOX_SELFUPDATE" is set. 
    // This is typically used to determine if the application is running in an update process.
    if (qEnvironmentVariableIsSet("SNIGDHAOS_BLACKBOX_SELFUPDATE")) {
        // If the environment variable is set, update the state to "SELECT" (presumably to indicate a state where the user can select an option).
        updateState(State::SELECT);
        return;  // Exit the function if the self-update process is active.
    }

    // Create a new QProcess object. This will be used to run external processes (such as the terminal command to update the system).
    auto process = new QProcess(this);

    // Create a temporary file that will be used during the update process.
    QTemporaryFile* file = new QTemporaryFile(this);
    file->open();  // Open the temporary file for writing.
    file->setAutoRemove(true);  // Set the temporary file to be automatically removed when it is closed.

    // Start a new process to launch the terminal and execute the system update command.
    // The command runs "sudo pacman -Syyu" to update the system and then deletes the temporary file after the update is done.
    // It also asks the user to press Enter before closing the terminal.
    process->start("/usr/lib/snigdhaos/launch-terminal", 
                    QStringList() << QString("sudo pacman -Syyu 2>&1 && rm \"" + file->fileName() + "\"; read -p 'Press Enter↵ to Exit'"));

    // Connect the finished signal of the QProcess to a lambda function, which will be executed when the process finishes.
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, [this, process, file](int exitcode, QProcess::ExitStatus status) {
        // Delete the QProcess and temporary file objects after the process finishes.
        process->deleteLater();
        file->deleteLater();

        // Check the exit code of the process:
        // If the exit code is 0 (successful), and the temporary file no longer exists, 
        // it indicates that the update process was successful, so relaunch the app with the state "POST_UPDATE".
        if (exitcode == 0 && !file->exists()) {
            relaunchSelf("POST_UPDATE");
        } else {
            // If the update failed (either the exit code is non-zero or the temporary file still exists),
            // relaunch the app with the state "UPDATE_RETRY", indicating that the update should be retried.
            relaunchSelf("UPDATE_RETRY");
        }
    });
}


void SnigdhaOSBlackbox::doApply() {
    // Declare three QStringLists to hold the packages, setup commands, and prepare commands
    QStringList packages;
    QStringList setup_commands;
    QStringList prepare_commands;

    // Find all QCheckBox widgets within the selectWidget_tabs widget
    auto checkBoxList = ui->selectWidget_tabs->findChildren<QCheckBox*>();

    // Iterate through each checkbox and check if it's checked
    for (auto checkbox : checkBoxList) {
        if (checkbox->isChecked()) {
            // If the checkbox is checked, retrieve its associated properties and add them to the lists
            packages += checkbox->property("packages").toStringList();  // Add selected package names to 'packages'
            setup_commands += checkbox->property("setup_commands").toStringList();  // Add setup commands to 'setup_commands'
            prepare_commands += checkbox->property("prepare_commands").toStringList();  // Add preparation commands to 'prepare_commands'
        }
    }

    // If no packages were selected, mark the state as 'SUCCESS' and exit early
    if (packages.isEmpty()) {
        updateState(State::SUCCESS);
        return;
    }

    // If 'podman' is selected in packages, add a system setup command for it
    if (packages.contains("podman")) {
        setup_commands += "systemctl enable --now podman.socket";
    }
    // If 'docker' is selected in packages, add a system setup command for it
    if (packages.contains("docker")) {
        setup_commands += "systemctl enable --now docker.socket";
    }

    // Remove duplicate entries in the 'packages' list to avoid redundant installations
    packages.removeDuplicates();

    // Create a temporary file to store the preparation commands and automatically delete it when no longer needed
    QTemporaryFile* prepareFile = new QTemporaryFile(this);
    prepareFile->setAutoRemove(true);  // Ensure this file is removed automatically when it goes out of scope
    prepareFile->open();  // Open the file for writing

    // Create a QTextStream to write the prepare commands to the temporary file
    QTextStream prepareStream(prepareFile);
    prepareStream << prepare_commands.join('\n');  // Join the list of prepare commands with newlines and write to the file
    prepareFile->close();  // Close the file after writing

    // Create another temporary file to store the selected packages
    QTemporaryFile* packagesFile = new QTemporaryFile(this);
    packagesFile->setAutoRemove(true);
    packagesFile->open();  // Open the file for writing

    // Create a QTextStream to write the list of packages to the temporary file
    QTextStream packagesStream(packagesFile);
    packagesStream << packages.join(' ');  // Join the package names with spaces and write to the file
    packagesFile->close();  // Close the file after writing

    // Create a third temporary file to store the setup commands
    QTemporaryFile* setupFile = new QTemporaryFile(this);
    setupFile->setAutoRemove(true);
    setupFile->open();  // Open the file for writing

    // Create a QTextStream to write the setup commands to the temporary file
    QTextStream setupStream(setupFile);
    setupStream << setup_commands.join('\n');  // Join the setup commands with newlines and write to the file
    setupFile->close();  // Close the file after writing

    // Create a QProcess to execute the shell script and pass the temporary file paths as arguments
    auto process = new QProcess(this);
    process->start("/usr/lib/snigdhaos/launch-terminal", 
                    QStringList() << QString("/usr/lib/snigdhaos-blackbox/apply.sh \"") + 
                    prepareFile->fileName() + "\" \"" + 
                    packagesFile->fileName() + "\" \"" + 
                    setupFile->fileName() + "\"");

    // When the process finishes, the following lambda function is triggered
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, [this, process, prepareFile, packagesFile, setupFile](int exitcode, QProcess::ExitStatus status) {
        
        // Clean up: delete the QProcess and the temporary files after the process finishes
        process->deleteLater();
        prepareFile->deleteLater();
        packagesFile->deleteLater();
        setupFile->deleteLater();

        // If the process was successful (exit code 0) and the temporary packages file no longer exists
        if (exitcode == 0 && !packagesFile->exists()) {
            // Mark the state as 'SELECT' to indicate the operation was successful
            updateState(State::SELECT);
        }
        else {
            // If there was an error (non-zero exit code or file exists), mark the state as 'APPLY_RETRY'
            updateState(State::APPLY_RETRY);
        }
    });
}

void SnigdhaOSBlackbox::populateSelectWidget() {
    // Check if the select widget already has more than one tab.
    // If so, there's no need to populate it again, so exit early.
    if (ui->selectWidget_tabs->count() > 1) {
        return;
    }

    // Retrieve the current desktop session environment variable.
    auto desktop = qEnvironmentVariable("XDG_DESKTOP_SESSION");

    // Set the visibility of the GNOME-related checkbox based on the desktop session.
    // The checkbox is visible only if the current session is GNOME.
    ui->checkBox_GNOME->setVisible(desktop == "gnome");

    // Variable to track whether the current device is a desktop.
    bool isDesktop = false;

    // Attempt to read the chassis type from the system's DMI data.
    auto chassis = QFile("/sys/class/dmi/id/chassis_type");
    if (chassis.open(QFile::ReadOnly)) { // Open the file in read-only mode.
        // List of chassis types that correspond to desktop systems.
        QStringList list = { "3", "4", "6", "7", "23", "24" };

        // Create a text stream to read data from the file.
        QTextStream in(&chassis);

        // Check if the read chassis type matches any in the list.
        isDesktop = list.contains(in.readLine());
    }

    // Set the visibility of the Performance checkbox based on whether
    // the system is identified as a desktop.
    ui->checkBox_Performance->setVisible(isDesktop);

    // Populate the select widget with entries from the specified file.
    // In this case, it's populating from "webapp.txt" with the label "WEBAPP".
    populateSelectWidget("/usr/lib/snigdhaos-blackbox/webapp.txt", "WEBAPP");
}

void SnigdhaOSBlackbox::populateSelectWidget(QString filename, QString label) {
    // Create a QFile object for the provided filename.
    QFile file(filename);

    // Check if the file can be opened in read-only mode.
    if (file.open(QIODevice::ReadOnly)) {
        // Create a QScrollArea to hold the new tab content.
        QScrollArea* scroll = new QScrollArea(ui->selectWidget_tabs);

        // Create a QWidget as the container for the scroll area.
        QWidget* tab = new QWidget(scroll);

        // Create a vertical layout for the container widget.
        QVBoxLayout* layout = new QVBoxLayout(tab);

        // Create a QTextStream to read from the file.
        QTextStream in(&file);

        // Loop through the file, reading three lines at a time.
        // Each set of lines corresponds to a checkbox definition.
        while (!in.atEnd()) {
            QString def = in.readLine();        // Line 1: Checkbox default state ("true" or "false").
            QString packages = in.readLine();  // Line 2: Associated package names (space-separated).
            QString display = in.readLine();   // Line 3: Display text for the checkbox.

            // Create a new QCheckBox and set its parent to the container widget.
            auto checkbox = new QCheckBox(tab);

            // Set the checkbox state based on the value in `def`.
            checkbox->setChecked(def == "true");

            // Set the display text for the checkbox.
            checkbox->setText(display);

            // Attach the package list as a custom property to the checkbox.
            // Splits the space-separated package string into a QStringList.
            checkbox->setProperty("packages", packages.split(" "));

            // Add the checkbox to the vertical layout.
            layout->addWidget(checkbox);
        }

        // Set the QWidget as the main content of the QScrollArea.
        scroll->setWidget(tab);

        // Add the QScrollArea as a new tab to the selectWidget_tabs,
        // using the provided label for the tab name.
        ui->selectWidget_tabs->addTab(scroll, label);

        // Close the file after reading its content.
        file.close();
    }
}

void SnigdhaOSBlackbox::updateState(State state) {
    // Only update the UI if the state has changed.
    if (currentState != state) {
        currentState = state;  // Update the current state.

        // Ensure the application window is visible and in focus.
        this->show();           // Make the window visible.
        this->activateWindow(); // Bring the window to the front.
        this->raise();          // Raise the window above others.

        // Handle the new state.
        switch (state) {
        case State::WELCOME:
            // Show the welcome screen.
            ui->mainStackedWidget->setCurrentWidget(ui->textWidget); // Switch to the text widget.
            ui->textStackedWidget->setCurrentWidget(ui->textWidget_welcome); // Show the welcome message.
            ui->textWidget_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); // Set appropriate buttons.
            break;

        case State::INTERNET:
            // Show the internet connection status screen.
            ui->mainStackedWidget->setCurrentWidget(ui->mainStackedWidget); // Switch to the main stack.
            ui->waitingWidget_text->setText("Waiting For Internet Connection..."); // Display waiting message.
            doInternetUpRequest(); // Trigger an internet connection check.
            break;

        case State::UPDATE:
            // Show the update progress screen.
            ui->mainStackedWidget->setCurrentWidget(ui->waitingWidget); // Switch to the waiting widget.
            ui->waitingWidget_text->setText("Please Wait! Till We Finish The Update..."); // Display update message.
            doUpdate(); // Start the update process.
            break;

        case State::UPDATE_RETRY:
            // Show the update retry screen.
            ui->mainStackedWidget->setCurrentWidget(ui->textWidget); // Switch to the text widget.
            ui->textStackedWidget->setCurrentWidget(ui->textWidget_updateRetry); // Show the retry message.
            ui->textWidget_buttonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No); // Set retry buttons.
            break;

        case State::QUIT:
            // Show the quit confirmation screen.
            ui->mainStackedWidget->setCurrentWidget(ui->textWidget); // Switch to the text widget.
            ui->textStackedWidget->setCurrentWidget(ui->textWidget_quit); // Show the quit message.
            ui->textWidget_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Reset); // Set quit buttons.
            break;

        case State::SELECT:
            // Show the selection screen.
            ui->mainStackedWidget->setCurrentWidget(ui->waitingWidget); // Switch to the waiting widget.
            populateSelectWidget(); // Populate the selection UI dynamically.
            break;

        case State::APPLY:
            // Show the apply changes screen.
            ui->mainStackedWidget->setCurrentWidget(ui->waitingWidget); // Switch to the waiting widget.
            ui->waitingWidget_text->setText("We are applying the changes..."); // Display applying message.
            doApply(); // Start applying changes.
            break;

        case State::APPLY_RETRY:
            // Show the apply retry screen.
            ui->mainStackedWidget->setCurrentWidget(ui->textWidget); // Switch to the text widget.
            ui->textStackedWidget->setCurrentWidget(ui->textWidget_applyRetry); // Show the retry message.
            ui->textWidget_buttonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Reset); // Set retry buttons.
            break;

        case State::SUCCESS:
            // Show the success screen.
            ui->mainStackedWidget->setCurrentWidget(ui->textWidget); // Switch to the text widget.
            ui->textStackedWidget->setCurrentWidget(ui->textWidget_success); // Show the success message.
            ui->textWidget_buttonBox->setStandardButtons(QDialogButtonBox::Ok); // Set success button.
            break;
        }
    }
}


void SnigdhaOSBlackbox::updateState(QString state) {
    if (state == "POST_UPDATE"){
        updateState(State::SELECT);
    }
    else if (state == "UPDATE_RETRY") {
        updateState(State::UPDATE_RETRY);
    }
    else {
        updateState(State::WELCOME);
    }
}

void SnigdhaOSBlackbox::relaunchSelf(QString param) {
    auto binary = QFileInfo(QCoreApplication::applicationFilePath());
    if (executable_modify_date != binary.lastModified()) {
        execlp(binary.absoluteFilePath().toUtf8().constData(), binary.fileName().toUtf8().constData(), param.toUtf8().constData(), NULL);
        exit(0);
    }
    else {
        updateState(param);
    }
}

void SnigdhaOSBlackbox::on_textWidget_buttonBox_clicked(QAbstractButton* button) {
    switch(currentState) {
    case State::WELCOME:
        if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Ok) {
            updateState(State::INTERNET);
        }
        break;

    case State::UPDATE_RETRY:
        if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Yes) {
            updateState(State::INTERNET);
        }
        break;

    case State::APPLY_RETRY:
        if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Yes) {
            updateState(State::APPLY);
        }
        else if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Reset) {
            updateState(State::SELECT);
        }
        break;

    case State::SUCCESS:
        if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Ok) {
            QApplication::quit();
        }
        break;

    case State::QUIT:
        if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::No || ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Ok) {
            QApplication::quit();
        }
        else {
            updateState(State::WELCOME);
        }
        break;
    default:;

    }
    if (ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::No || ui->textWidget_buttonBox->standardButton(button) == QDialogButtonBox::Cancel) {
        updateState(State::QUIT);
    }
}

void SnigdhaOSBlackbox::on_selectWidget_buttonBox_Clicked(QAbstractButton* button) {
    if (ui->selectWidget_buttonBox->standardButton(button) == QDialogButtonBox::Ok) {
        updateState(State::APPLY);
    }
    else {
        updateState(State::QUIT);
    }
}
