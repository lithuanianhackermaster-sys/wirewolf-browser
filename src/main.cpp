#include <QApplication>
#include <QMainWindow>
#include <QLineEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QIcon>
#include <QUrl>
#include <QTabWidget>
#include <QAction>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

// WebView2 headers
#include <wil/com.h>
#include <WebView2.h>

// Qt helper widget hosting WebView2
class WebView2Widget : public QWidget
{
    Q_OBJECT
public:
    WebView2Widget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow);
        initialize();
    }

    void loadURL(const QString& url)
    {
        if (m_webview) {
            std::wstring wurl = url.toStdWString();
            m_webview->Navigate(wurl.c_str());
        }
    }
    void setPrivate(bool priv) {
        m_private = priv;
        if (m_webview) {
            wil::com_ptr<ICoreWebView2Settings> settings;
            m_webview->get_Settings(&settings);
            if (settings)
                settings->put_IsInPrivateModeEnabled(priv ? TRUE : FALSE);
        }
    }
    void goBack() {
        if (m_webview) m_webview->GoBack();
    }
    void goForward() {
        if (m_webview) m_webview->GoForward();
    }
    void reload() {
        if (m_webview) m_webview->Reload();
    }
    void setJavaScriptEnabled(bool enabled) {
        if (m_webview) {
            wil::com_ptr<ICoreWebView2Settings> settings;
            m_webview->get_Settings(&settings);
            if (settings) settings->put_IsScriptEnabled(enabled ? TRUE : FALSE);
        }
    }
    void setCookiesEnabled(bool enabled) {
        if (m_webview) {
            wil::com_ptr<ICoreWebView2Settings> settings;
            m_webview->get_Settings(&settings);
            if (settings) settings->put_AreDefaultScriptDialogsEnabled(enabled ? TRUE : FALSE);
            // WebView2 does not have a direct cookie toggle; real implementation
            // would require clearing or using network filters. This is a placeholder.
        }
    }


signals:
    void urlChanged(const QString &url);

private:
    wil::com_ptr<ICoreWebView2Controller> m_controller;
    wil::com_ptr<ICoreWebView2> m_webview;
    bool m_private = false;

    void initialize()
    {
        HWND hwnd = (HWND)winId();
        // create environment
        CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (result != S_OK || env == nullptr)
                        return S_OK;

                    env->CreateCoreWebView2Controller(
                        hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (controller) {
                                    m_controller = controller;
                                    controller->get_CoreWebView2(&m_webview);
                                    if (m_private && m_webview) {
                                        wil::com_ptr<ICoreWebView2Settings> settings;
                                        m_webview->get_Settings(&settings);
                                        if (settings)
                                            settings->put_IsInPrivateModeEnabled(TRUE);
                                    }
                                    if (m_webview) {
                                        // notify when URL changes
                                        m_webview->add_NavigationCompleted(
                                            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                                [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                                    wil::unique_cotaskmem_string uri;
                                                    m_webview->get_Source(&uri);
                                                    QString quri = QString::fromWCharArray(uri.get());
                                                    emit urlChanged(quri);
                                                    return S_OK;
                                                }
                                            ).Get());
                                    }
                                }
                                return S_OK;
                            }
                        ).Get());
                    return S_OK;
                }
            ).Get());
    }
};

// MainWindow with tab support and styled UI
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent=nullptr) : QMainWindow(parent), m_privateMode(false) {
        setWindowTitle("Wirewolf");
        setWindowIcon(QIcon(":/icons/logo.png"));
        resize(1024, 768);

        // central widget contains a vertical layout
        QWidget *central = new QWidget;
        QVBoxLayout *vlay = new QVBoxLayout(central);
        vlay->setContentsMargins(0,0,0,0);
        vlay->setSpacing(0);

        // address bar on top
        addressBar = new QLineEdit;
        addressBar->setPlaceholderText("Search or enter a URL");
        addressBar->setObjectName("addressBar");
        connect(addressBar, &QLineEdit::returnPressed, this, &MainWindow::navigateCurrentTab);

        // toolbar with navigation controls and new‑tab button
        QToolBar *toolbar = new QToolBar;
        QAction *backAct = toolbar->addAction("⬅");
        QAction *forwardAct = toolbar->addAction("➡");
        QAction *reloadAct = toolbar->addAction("⟳");
        QAction *homeAct = toolbar->addAction("🏠");
        toolbar->addWidget(addressBar);
        QAction *newTabAct = toolbar->addAction("+");

        connect(backAct, &QAction::triggered, this, &MainWindow::goBack);
        connect(forwardAct, &QAction::triggered, this, &MainWindow::goForward);
        connect(reloadAct, &QAction::triggered, this, &MainWindow::reloadPage);
        connect(homeAct, &QAction::triggered, this, &MainWindow::goHome);
        connect(newTabAct, &QAction::triggered, this, &MainWindow::addTab);

        // file menu
        QMenu *fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction("Import Chrome Logins", this, &MainWindow::importChromeLogins);

        // security / privacy menu
        QMenu *secMenu = menuBar()->addMenu("&Security");
        QAction *jsAct = secMenu->addAction("Enable JavaScript");
        jsAct->setCheckable(true);
        jsAct->setChecked(true);
        connect(jsAct, &QAction::toggled, this, &MainWindow::toggleJavaScript);
        QAction *cookiesAct = secMenu->addAction("Enable Cookies");
        cookiesAct->setCheckable(true);
        cookiesAct->setChecked(true);
        connect(cookiesAct, &QAction::toggled, this, &MainWindow::toggleCookies);
        QAction *privateAct = secMenu->addAction("New tabs are private");
        privateAct->setCheckable(true);
        privateAct->setChecked(false);
        connect(privateAct, &QAction::toggled, this, &MainWindow::setPrivateMode);
        secMenu->addSeparator();
        secMenu->addAction("Clear History", this, &MainWindow::clearHistory);

        vlay->addWidget(toolbar);

        // tab widget
        tabs = new QTabWidget;
        tabs->setTabsClosable(true);
        connect(tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
        connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::updateAddressBar);

        vlay->addWidget(tabs);
        setCentralWidget(central);

        // apply dark stylesheet
        setStyleSheet(
            "QMainWindow { background: #1e1e2f; }"
            "QToolBar { background: #2b2b3d; spacing: 4px; }"
            "QToolButton { background: transparent; color: #ddd; border: none; padding: 4px; }"
            "QToolButton:hover { background: #3a3a4f; }"
            "QLineEdit#addressBar { border-radius: 12px; padding: 6px; background: #3a3a4f; color: #ffffff; }"
            "QTabBar::tab { background: #2b2b3d; color: #ddd; padding: 6px; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right:2px; }"
            "QTabBar::tab:selected { background: #3a3a4f; color: #fff; }"
        );

        addTab();
    }

private:
    bool m_privateMode;
private slots:
    void addTab() {
        WebView2Widget *view = new WebView2Widget;
        if (m_privateMode) {
            view->setPrivate(true);
        }
        int index = tabs->addTab(view, "New Tab");
        tabs->setCurrentIndex(index);
        view->loadURL("https://duckduckgo.com");
        connect(view, &WebView2Widget::urlChanged, [this,index](const QString &url){
            tabs->setTabText(index, url);
            if (tabs->currentIndex() == index)
                addressBar->setText(url);
        });
    }

    void closeTab(int index) {
        QWidget *w = tabs->widget(index);
        tabs->removeTab(index);
        delete w;
        if (tabs->count() == 0) addTab();
    }

    void updateAddressBar(int index) {
        WebView2Widget *view = qobject_cast<WebView2Widget*>(tabs->widget(index));
        if (view) {
            // we could store last-known URL in view and signal on change; for now
            // just clear the address bar until navigation completes
            addressBar->setText("");
        }
    }

    void navigateCurrentTab() {
        QString url = addressBar->text();
        if (!url.startsWith("http")) {
            url = QString("https://duckduckgo.com/?q=%1").arg(QUrl::toPercentEncoding(addressBar->text()));
        }
        WebView2Widget *view = qobject_cast<WebView2Widget*>(tabs->currentWidget());
        if (view) view->loadURL(url);
    }

    void goBack() {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->goBack();
    }
    void goForward() {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->goForward();
    }
    void reloadPage() {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->reload();
    }
    void goHome() {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->loadURL("https://duckduckgo.com");
    }

    void toggleJavaScript(bool enabled) {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->setJavaScriptEnabled(enabled);
    }

    void setPrivateMode(bool on);
    void clearHistory();
    void toggleCookies(bool enabled) {
        if (auto view = qobject_cast<WebView2Widget*>(tabs->currentWidget()))
            view->setCookiesEnabled(enabled);
    }

    void setPrivateMode(bool on) {
        m_privateMode = on;
    }

    void clearHistory() {
        // simple clear: remove all tabs and reopen home
        while (tabs->count() > 0) {
            QWidget *w = tabs->widget(0);
            tabs->removeTab(0);
            delete w;
        }
        addTab();
    }

    // ---- import helpers -------------------------------------------------
    QByteArray decryptDpapi(const QByteArray &blob) {
        DATA_BLOB in;
        in.pbData = (BYTE*)blob.data();
        in.cbData = blob.size();
        DATA_BLOB out;
        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
            QByteArray result((char*)out.pbData, out.cbData);
            LocalFree(out.pbData);
            return result;
        }
        return {};
    }

    void importChromeLogins() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Chrome profile directory");
        if (dir.isEmpty())
            return;
        QString dbFile = dir + "/Login Data";
        if (!QFile::exists(dbFile)) {
            QMessageBox::warning(this, "Import", "Login Data file not found");
            return;
        }
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "import");
        db.setDatabaseName(dbFile);
        if (!db.open()) {
            QMessageBox::warning(this, "Import", "Failed to open database");
            return;
        }
        QSqlQuery q(db, "SELECT origin_url, username_value, password_value FROM logins");
        while (q.next()) {
            QString url = q.value(0).toString();
            QString user = q.value(1).toString();
            QByteArray enc = q.value(2).toByteArray();
            QByteArray pwd = decryptDpapi(enc);
            qDebug() << "imported" << url << user << pwd;
        }
        db.close();
        QSqlDatabase::removeDatabase("import");
        QMessageBox::information(this, "Import", "Chrome logins scanned (see debug output)");
    }

private:
    QTabWidget *tabs;
    QLineEdit *addressBar;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();
    return app.exec();
}

