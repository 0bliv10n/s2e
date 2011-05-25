extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
#include <tcg/tcg.h>
#include <tcg/tcg-llvm.h>
}

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Linker.h>

#include <llvm/System/Path.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/ModuleProvider.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/target/TargetData.h>

#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>


#include <lib/BinaryReaders/BFDInterface.h>
#include <lib/Utils/Log.h>

#include "StaticTranslator.h"
#include "Passes/ConstantExtractor.h"
#include "Passes/JumpTableExtractor.h"
#include "Passes/FunctionBuilder.h"
#include "Passes/CallBuilder.h"

#include "lib/Utils/Utils.h"

using namespace llvm;
using namespace s2etools;
using namespace s2etools::translator;
using namespace s2e::plugins;

struct MyQwordParser : public cl::basic_parser<uint64_t> {
  // parse - Return true on error.
  bool parse(cl::Option &O, const char *ArgName, const std::string &Arg,
             uint64_t &Val) {
      const char *ArgStart = Arg.c_str();
      char *End;
      Val = strtol(ArgStart, &End, 0);
//      std::cout << "ArgStart=" << ArgStart << " val=" << std::hex << Val << std::endl;
      return false;
  }
};



namespace {
cl::opt<std::string>
    InputFile(cl::Positional, cl::Required, cl::desc("<input file>"));

cl::opt<std::string>
    BitcodeLibrary("bitcodelibrary", cl::Required, cl::desc("Translator bitcode file"));

cl::opt<std::string>
    OutputDir("outputdir", cl::desc("Store the analysis output in this directory"), cl::init("."));

cl::opt<bool>
    ExpMode("expmode", cl::desc("Auto-increment the x2e-out-* folder and create x2l-last symlink"), cl::init(false));


cl::opt<std::string>
    BfdFormat("bfd", cl::desc("Binary format of the input (in case auto detection fails)"), cl::init(""));

cl::opt<uint64_t, false, MyQwordParser >
    EntryPointAddress("entrypoint", cl::desc("<address> Override the address of the default entry point"), cl::init(0));

}

namespace s2etools {
namespace translator {
static BFDInterface *s_currentBinary = NULL;
LogKey StaticTranslatorTool::TAG = LogKey("StaticTranslatorTool");

bool StaticTranslatorTool::s_translatorInited = false;

StaticTranslatorTool::StaticTranslatorTool()
{
    if (ExpMode) {
        m_experiment = new ExperimentManager(OutputDir, "x2l");
    }else {
        m_experiment = new ExperimentManager(OutputDir);
    }

    m_startTime = llvm::sys::TimeValue::now().usec();

    m_bfd = new BFDInterface(InputFile, false);
    if (!m_bfd->initialize(BfdFormat)) {
        LOGDEBUG() << "Could not open " << InputFile << std::endl;
        exit(-1);
    }

    m_binary = m_bfd->getBinary();

    llvm::sys::Path libraryPath(BitcodeLibrary);
    m_translator = new X86Translator(libraryPath);
    m_translator->setBinaryFile(m_binary);
    m_translator->setSingleStep(true);
    m_translatedCode = m_experiment->getOuputFile("translated.bin");
}

//XXX: the translator is global...
StaticTranslatorTool::~StaticTranslatorTool()
{
    delete m_translatedCode;
    delete m_experiment;

    s_currentBinary = NULL;

    if (m_bfd) {
        delete m_bfd;
    }
    m_binary = NULL;

    delete m_translator;

}

void StaticTranslatorTool::computePredecessors()
{
    foreach(it, m_translatedInstructions.begin(), m_translatedInstructions.end()) {
        TranslatedBlock *tb = (*it).second;
        if (!m_predecessors.count(tb->getAddress())) {
            m_predecessors[tb->getAddress()] = 0;
        }

        if (tb->isCallInstruction()) {
            ConstantInt *ci = dyn_cast<ConstantInt>(tb->getFallback());
            assert(ci);
            m_predecessors[ci->getZExtValue()] = tb->getAddress();
        }else {
            const TranslatedBlock::Successors &sucs = tb->getSuccessors();
            foreach(sit, sucs.begin(), sucs.end()) {
                ConstantInt *ci;
                if (*sit && (ci = dyn_cast<ConstantInt>(*sit))) {
                    ++m_predecessors[ci->getZExtValue()];
                }
            }
        }
    }
}

//Retrieve all program counters with no predecessors
void StaticTranslatorTool::computeFunctionEntryPoints(AddressSet &entryPoints)
{
    foreach(it, m_predecessors.begin(), m_predecessors.end()) {
        if ((*it).second == 0) {
            entryPoints.insert((*it).first);
        }
    }
}

//Get the list of all instructions in a function
//XXX: call to the next program counter
void StaticTranslatorTool::computeFunctionInstructions(uint64_t entryPoint, AddressSet &exploredInstructions)
{    
    AddressSet instructionsToExplore;

    exploredInstructions.clear();
    instructionsToExplore.insert(entryPoint);
    while(!instructionsToExplore.empty()) {
        uint64_t addr = *instructionsToExplore.begin();
        instructionsToExplore.erase(addr);
        if (exploredInstructions.count(addr)) {
            continue;
        }
        exploredInstructions.insert(addr);

        TranslatedBlock *tb = m_translatedInstructions[addr];
        assert(tb && "There must be a corresponding LLVM translation for each instruction");

        if (tb->isCallInstruction()) {
            ConstantInt *ci = dyn_cast<ConstantInt>(tb->getFallback());
            assert(ci);
            instructionsToExplore.insert(ci->getZExtValue());
        }else {
            const TranslatedBlock::Successors &sucs = tb->getSuccessors();
            foreach(sit, sucs.begin(), sucs.end()) {
                ConstantInt *ci;
                if (*sit && (ci = dyn_cast<ConstantInt>(*sit))) {
                    instructionsToExplore.insert(ci->getZExtValue());
                }
            }
        }
    }
}

void StaticTranslatorTool::translateAllInstructions()
{
    uint64_t ep = getEntryPoint();

    if (!ep) {
        LOGERROR() << "Could not get entry point of " << InputFile << std::endl;
        exit(-1);
    }

    m_addressesToExplore.insert(ep);
    while(!m_addressesToExplore.empty()) {
        uint64_t addr = *m_addressesToExplore.begin();
        m_addressesToExplore.erase(addr);

        if (m_exploredAddresses.count(addr)) {
            continue;
        }
        m_exploredAddresses.insert(addr);

        LOGDEBUG() << "L: Translating at address 0x" << std::hex << addr << std::endl;

        TranslatedBlock *bblock = NULL;
        try {
            bblock = m_translator->translate(addr);
        }catch(InvalidAddressException &e) {
            LOGERROR() << "Could not access address 0x" << std::hex << e.getAddress() << std::endl;
            continue;
        }

        assert(bblock);
        m_translatedInstructions[addr] = bblock;

        const TranslatedBlock::Successors &sucs = bblock->getSuccessors();
        foreach(sit, sucs.begin(), sucs.end()) {
            ConstantInt *ci;
            if (*sit && (ci = dyn_cast<ConstantInt>(*sit))) {
                m_addressesToExplore.insert(ci->getZExtValue());
            }
        }

        extractAddresses(bblock->getFunction(), bblock->isIndirectJump());

    }

    LOGINFO() << "There are " << std::dec << m_translatedInstructions.size() << " instructions" << std::endl;

}

bool StaticTranslatorTool::checkString(uint64_t address, std::string &res, bool isUnicode)
{
    std::string ret;
    unsigned char c;
    uint16_t u;

    do {
        if (isUnicode) {
            if (!m_binary->read(address, &u, sizeof(u))) {
                return false;
            }
        }else {
            if (!m_binary->read(address, &c, sizeof(c))) {
                return false;
            }
            u = c;
        }

        if (u > 0 && (u < 0x20 || u >= 0x80) && u != 0xd && u != 0xa) {
            return false;
        }
        if (u) {
            ret = ret + (char)u;
        }

        address = isUnicode ? address + 2 : address + 1;
    }while(u);

    //XXX: Improve the heuristic
    if (ret.size() < 2) {
        return false;
    }

    res = ret;
    return true;
}

void StaticTranslatorTool::extractAddresses(llvm::Function *llvmInstruction, bool isIndirectJump)
{
    JumpTableExtractor jumpTableExtractor;
    uint64_t jumpTableAddress = 0;

    if (isIndirectJump) {
        jumpTableExtractor.runOnFunction(*llvmInstruction);
        jumpTableAddress = jumpTableExtractor.getJumpTableAddress();
        if (jumpTableAddress) {
            LOGDEBUG() << "Found jump table at 0x" << std::hex << jumpTableAddress << std::endl;
        }
    }

    ConstantExtractor extractor;
    extractor.runOnFunction(*llvmInstruction);

    const ConstantExtractor::Constants &consts = extractor.getConstants();
    foreach(it, consts.begin(), consts.end()) {
        uint64_t addr = *it;
        //Disacard anything that falls outside the binary
        if (!m_bfd->isCode(addr)) {
            continue;
        }

#if 0
        //Skip if the address falls inside the current bb
        if (addr >= bb->getAddress() && (addr < bb->getAddress() + bb->getSize())) {
            continue;
        }
#endif

        //Skip jump tables
        if (addr == jumpTableAddress) {
            continue;
        }

        //Skip strings
        std::string str;
        if (checkString(addr, str, false) || checkString(addr, str, true)) {
            LOGDEBUG() << "Found string at 0x" << std::hex << addr << ": " << str << std::endl;
            continue;
        }

        LOGDEBUG() << "L: Found new address 0x" << std::hex << addr << std::endl;
        m_addressesToExplore.insert(addr);
    }
}

void StaticTranslatorTool::reconstructFunctions(const AddressSet &entryPoints)
{
    AddressSet alreadyExplored;

    foreach(it, entryPoints.begin(), entryPoints.end()) {
        uint64_t ep = *it;
        StaticTranslatorTool::AddressSet instructions;
        computeFunctionInstructions(ep, instructions);

        LOGDEBUG() << "EP: 0x" << std::hex << ep << std::endl;
        foreach(iit, instructions.begin(), instructions.end()) {
            if (alreadyExplored.count(*iit)) {
                LOGERROR() << "Instruction 0x" << *iit << " of function 0x" << std::hex << ep << " already in another function" << std::endl;
            }
            alreadyExplored.insert(*iit);
            LOGDEBUG() << "    " << std::hex << *iit << std::endl;
        }

        FunctionBuilder functionBuilder(m_translatedInstructions, instructions, ep);
        functionBuilder.runOnModule(*m_translator->getModule());

        FunctionPassManager fpm(m_translator->getModuleProvider());
        fpm.add(createVerifierPass());
        fpm.run(*functionBuilder.getFunction());
    }
}

void StaticTranslatorTool::reconstructFunctionCalls()
{
    CallBuilder callBuilder(m_binary);
    callBuilder.runOnModule(*m_translator->getModule());

    FunctionPassManager fpm(m_translator->getModuleProvider());
    fpm.add(createVerifierPass());
    Module *M = m_translator->getModule();
    foreach(it, M->begin(), M->end()) {
        fpm.run(*it);
    }
}

void StaticTranslatorTool::outputBitcodeFile()
{

    std::ostream *o = m_experiment->getOuputFile("module.bc");

    llvm::Module *module = tcg_llvm_ctx->getModule();
    module->setTargetTriple("i386-apple-darwin10.4");
    module->setDataLayout("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128");

    // Output the bitcode file to stdout
    llvm::WriteBitcodeToFile(module, *o);
    delete o;
}

uint64_t StaticTranslatorTool::getEntryPoint()
{
    if (EntryPointAddress) {
        return EntryPointAddress;
    }else {
        return m_binary->getEntryPoint();
    }
}

void StaticTranslatorTool::dumpStats()
{
    Module *m = m_translator->getModule();
    std::ostream *bbFile = m_experiment->getOuputFile("stats.txt");
    unsigned hardCodedMemAccesses = 0;
    unsigned totalMemAccesses = 0;

    llvm::DenseSet<Function*> memFunctions;
    memFunctions.insert(m->getFunction("__ldb_mmu"));
    memFunctions.insert(m->getFunction("__ldw_mmu"));
    memFunctions.insert(m->getFunction("__ldl_mmu"));
    memFunctions.insert(m->getFunction("__ldq_mmu"));

    memFunctions.insert(m->getFunction("__stb_mmu"));
    memFunctions.insert(m->getFunction("__stw_mmu"));
    memFunctions.insert(m->getFunction("__stl_mmu"));
    memFunctions.insert(m->getFunction("__stq_mmu"));

    foreach(it, memFunctions.begin(), memFunctions.end()) {
        Function *f = (*it);
        foreach(uit, f->use_begin(), f->use_end()) {
            CallInst *ci = dyn_cast<CallInst>(*uit);
            if (ci) {
                if (dyn_cast<ConstantInt>(ci->getOperand(1))) {
                    ++hardCodedMemAccesses;
                }
            }
            ++totalMemAccesses;
        }
    }

    *bbFile << "TotalMemoryAccesses:     " << std::dec << totalMemAccesses << std::endl;
    *bbFile << "HardCodedMemoryAccesses: " << std::dec << hardCodedMemAccesses << std::endl;
    *bbFile << "TotalRelocations:        " << std::dec << m_binary->getRelocations().size() << std::endl;

    delete bbFile;

#if 0

    std::ostream *bbFile = m_experiment->getOuputFile("bblist.txt");

    foreach(it, m_exploredAddresses.begin(), m_exploredAddresses.end()) {
        *bbFile << std::hex << "0x" << *it << std::endl;
    }

    std::ostream *fcnFile = m_experiment->getOuputFile("functions.txt");


    foreach(it, m_functions.begin(), m_functions.end()) {
        CFunction *fcn = *it;
        *fcnFile << std::hex << "0x" << fcn->getAddress() << std::endl;
    }


    m_endTime = llvm::sys::TimeValue::now().usec();
    std::ostream *statsFile = m_experiment->getOuputFile("stats.txt");

    *statsFile << "Execution time: " << std::dec << (m_endTime - m_startTime) / 1000000.0 << std::endl;
    *statsFile << "Instructions :  " << m_exploredAddresses.size() << std::endl;
    *statsFile << "Functions:      " << m_functions.size() << std::endl;

    delete statsFile;
    delete fcnFile;
    delete bbFile;
#endif
}

}
}


int main(int argc, char** argv)
{
    cl::ParseCommandLineOptions(argc, (char**) argv);

    StaticTranslatorTool translator;
    StaticTranslatorTool::AddressSet entryPoints;

    translator.translateAllInstructions();
    translator.computePredecessors();
    translator.computeFunctionEntryPoints(entryPoints);
    translator.reconstructFunctions(entryPoints);
    translator.reconstructFunctionCalls();

#if 0
    translator.exploreBasicBlocks();
    translator.extractFunctions();


    if (translator.getFunctions().size() == 0) {
        std::cout << "No functions found" << std::endl;
        return -1;
    }

    translator.cleanupCode();
    translator.renameEntryPoint();
#endif
    translator.outputBitcodeFile();

    translator.dumpStats();

    return 0;
}
