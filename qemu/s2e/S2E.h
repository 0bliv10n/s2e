#ifndef S2E_H
#define S2E_H

#include <fstream>
#include <string>
#include <vector>
//#include <tr1/unordered_map>
#include <map>

#include "Plugin.h"

namespace klee {
    class Interpreter;
    class InterpreterHandler;
}

class TCGLLVMContext;

namespace s2e {

class Plugin;
class CorePlugin;
class ConfigFile;
class PluginsFactory;

class S2EHandler;
class S2EExecutor;
class S2EExecutionState;

class Database;

class S2E
{
protected:
    ConfigFile* m_configFile;
    PluginsFactory* m_pluginsFactory;

    CorePlugin* m_corePlugin;
    std::vector<Plugin*> m_activePluginsList;

    typedef std::map<std::string, Plugin*> ActivePluginsMap;
    ActivePluginsMap m_activePluginsMap;

    std::string m_outputDirectory;

    std::ostream*   m_infoFile;

    std::ostream*   m_debugFile;

    std::ostream*   m_messagesFile;
    std::streambuf* m_messagesStreamBuf;

    std::ostream*   m_warningsFile;
    std::streambuf* m_warningsStreamBuf;

    Database *m_database;

    TCGLLVMContext *m_tcgLLVMContext;

    /* The following members are late-initialized when
    QEMU pc creation is complete */
    S2EHandler* m_s2eHandler;
    S2EExecutor* m_s2eExecutor;

    void initOutputDirectory(const std::string& outputDirectory);
    void initKleeOptions();
    void initExecutor();
    void initPlugins();

    std::ostream& getStream(std::ostream& stream,
                            const S2EExecutionState* state) const;

public:
    /** Construct S2E */
    explicit S2E(int argc, char** argv,
                 TCGLLVMContext* tcgLLVMContext,
                 const std::string& configFileName,
                 const std::string& outputDirectory);
    ~S2E();

    /*****************************/
    /* Configuration and plugins */

    /** Get configuration file */
    ConfigFile* getConfig() const { return m_configFile; }

    /** Get plugin by name of functionName */
    Plugin* getPlugin(const std::string& name) const;

    /** Get Core plugin */
    CorePlugin* getCorePlugin() const { return m_corePlugin; }

    /** Get database */
    Database *getDb() const {
        return m_database;
    }

    /*************************/
    /* Directories and files */

    /** Get output directory name */
    const std::string& getOutputDirectory() const { return m_outputDirectory; }

    /** Get a filename inside an output directory */
    std::string getOutputFilename(const std::string& fileName);

    /** Create output file in an output directory */
    std::ostream* openOutputFile(const std::string &filename);

    /** Get info stream (used only by KLEE internals) */
    std::ostream& getInfoStream(const S2EExecutionState* state = 0) const {
        return getStream(*m_infoFile, state);
    }

    /** Get debug stream (used for non-important debug info) */
    std::ostream& getDebugStream(const S2EExecutionState* state = 0) const {
        return getStream(*m_debugFile, state);
    }

    static void printf(std::ostream &os, const char *fmt, ...);

    /** Get messages stream (used for non-critical information) */
    std::ostream& getMessagesStream(const S2EExecutionState* state = 0) const {
        return getStream(*m_messagesFile, state);
    }

    /** Get warnings stream (used for warnings, duplicated on the screen) */
    std::ostream& getWarningsStream(const S2EExecutionState* state = 0) const {
        return getStream(*m_warningsFile, state);
    }

    /***********************/
    /* Runtime information */
    S2EExecutor* getExecutor() { return m_s2eExecutor; }
};

} // namespace s2e

#endif // S2E_H
