#include "qt/info_dialog.hpp"
#include "qt/mainwindow.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "coding/file_reader.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#include "std/cstdio.hpp"
#include "std/cstdlib.hpp"
#include "std/sstream.hpp"

#include "3party/Alohalytics/src/alohalytics.h"
#include "3party/gflags/src/gflags/gflags.h"

#include <QtCore/QDir>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  #include <QtGui/QApplication>
#else
  #include <QtWidgets/QApplication>
#endif

DEFINE_string(log_abort_level, my::ToString(my::GetDefaultLogAbortLevel()),
              "Log messages severity that causes termination.");

namespace
{
bool ValidateLogAbortLevel(char const * flagname, string const & value)
{
  my::LogLevel level;
  if (!my::FromString(value, level))
  {
    ostringstream os;
    auto const & names = my::GetLogLevelNames();
    for (size_t i = 0; i < names.size(); ++i)
    {
      if (i != 0)
        os << ", ";
      os << names[i];
    }

    printf("Invalid value for --%s: %s, must be one of: %s\n", flagname, value.c_str(),
           os.str().c_str());
    return false;
  }
  return true;
}

bool const g_logAbortLevelDummy =
    google::RegisterFlagValidator(&FLAGS_log_abort_level, &ValidateLogAbortLevel);

class FinalizeBase
{
public:
  ~FinalizeBase()
  {
    // optional - clean allocated data in protobuf library
    // useful when using memory and resource leak utilites
    // google::protobuf::ShutdownProtobufLibrary();
  }
  };

#if defined(OMIM_OS_WINDOWS) //&& defined(PROFILER_COMMON)
  class InitializeFinalize : public FinalizeBase
  {
    FILE * m_errFile;
    my::ScopedLogLevelChanger const m_debugLog;
  public:
    InitializeFinalize() : m_debugLog(LDEBUG)
    {
      // App runs without error console under win32.
      m_errFile = ::freopen(".\\mapsme.log", "w", stderr);

      //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF);
      //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
    ~InitializeFinalize()
    {
      ::fclose(m_errFile);
    }
  };
#else
  typedef FinalizeBase InitializeFinalize;
#endif
}  // namespace

int main(int argc, char * argv[])
{
  google::SetUsageMessage("Desktop application.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  my::LogLevel level;
  CHECK(my::FromString(FLAGS_log_abort_level, level), ());
  my::g_LogAbortLevel = level;

  Q_INIT_RESOURCE(resources_common);

  // Our double parsing code (base/string_utils.hpp) needs dots as a floating point delimiters, not commas.
  // TODO: Refactor our doubles parsing code to use locale-independent delimiters.
  // For example, https://github.com/google/double-conversion can be used.
  // See http://dbaron.org/log/20121222-locale for more details.
  (void)::setenv("LC_NUMERIC", "C", 1);

  InitializeFinalize mainGuard;
  UNUSED_VALUE(mainGuard);

  QApplication a(argc, argv);

#ifdef DEBUG
  alohalytics::Stats::Instance().SetDebugMode(true);
#endif

  GetPlatform().SetupMeasurementSystem();

  // display EULA if needed
  char const * settingsEULA = "EulaAccepted";
  bool eulaAccepted = false;
  if (!settings::Get(settingsEULA, eulaAccepted) || !eulaAccepted)
  {
    QStringList buttons;
    buttons << "Accept" << "Decline";

    string buffer;
    {
      ReaderPtr<Reader> reader = GetPlatform().GetReader("eula.html");
      reader.ReadAsString(buffer);
    }
    qt::InfoDialog eulaDialog("MAPS.ME End User Licensing Agreement", buffer.c_str(), NULL, buttons);
    eulaAccepted = (eulaDialog.exec() == 1);
    settings::Set(settingsEULA, eulaAccepted);
  }

  int returnCode = -1;
  if (eulaAccepted)   // User has accepted EULA
  {
    bool apiOpenGLES3 = false;
#if defined(OMIM_OS_MAC)
    apiOpenGLES3 = a.arguments().contains("es3", Qt::CaseInsensitive);
#endif
    qt::MainWindow::SetDefaultSurfaceFormat(apiOpenGLES3);

    Framework framework;
    qt::MainWindow w(framework, apiOpenGLES3);
    w.show();
    returnCode = a.exec();
  }

  LOG_SHORT(LINFO, ("MapsWithMe finished with code", returnCode));
  return returnCode;
}
