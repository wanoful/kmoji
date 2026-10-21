#include <QApplication>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QWindow>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qwindowsysteminterface.h>
#include <cstdio>
int main(int argc,char **argv) {
 QApplication app(argc,argv); app.setApplicationName("kmoji-real-input-check");
 QWidget target;target.setWindowTitle("kMoji input verification");target.resize(440,120);
 QVBoxLayout layout(&target);QLabel label("Testing native emoji input in this temporary field…");layout.addWidget(&label);
 QLineEdit edit; edit.setText(QString::fromUtf8("验证：")); QString baseline; layout.addWidget(&edit);target.show();edit.setFocus();
 auto key=[&](int k,int code,int sym,Qt::KeyboardModifiers mods=Qt::NoModifier,const QString &text=QString(),int nativeMods=0){
  auto *context=QGuiApplicationPrivate::platformIntegration()->inputContext();
  for(auto t:{QEvent::KeyPress,QEvent::KeyRelease}){
   QKeyEvent e(t,k,mods,code,sym,nativeMods,text);
   bool accepted=context && context->filterEvent(&e);
   std::printf("key %d event %d accepted %d\n",k,int(t),accepted); std::fflush(stdout);
   if(!accepted) QApplication::sendEvent(&edit,&e);
  }
 };
 QTimer::singleShot(800,[&]{
  std::printf("focused %d active %d\n",edit.hasFocus(),target.isActiveWindow());std::fflush(stdout);
  key(Qt::Key_Period,60,46,Qt::MetaModifier,".",64);
 });
 QTimer::singleShot(1700,[&]{key(Qt::Key_C,54,'c',Qt::NoModifier,"c");});
 QTimer::singleShot(1800,[&]{key(Qt::Key_A,38,'a',Qt::NoModifier,"a");});
 QTimer::singleShot(1900,[&]{key(Qt::Key_T,28,'t',Qt::NoModifier,"t");});
 QTimer::singleShot(2150,[&]{baseline=edit.text();key(Qt::Key_Return,36,0xff0d);});
 QTimer::singleShot(2500,[&]{
  std::printf("Real Qt text field: %s\n",edit.text().toUtf8().constData());std::fflush(stdout);
  app.exit(edit.text()==baseline+QString::fromUtf8("🐈")?0:1);
 });
 return app.exec();
}
