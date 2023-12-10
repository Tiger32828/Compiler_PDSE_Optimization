//===-- Partial Dead Store Elimination Pass --------------------===//
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include <unordered_set>
#include <stack>

/* *******Implementation Starts Here******* */
// You can include more Header files here
/* *******Implementation Ends Here******* */

using namespace llvm;

namespace
{
  struct PDSECorrectnessPass : public PassInfoMixin<PDSECorrectnessPass>
  {

    struct bbSinkingAnalysis
    {
      Instruction *gen = nullptr;
      bool kill = false;
      Value *in = nullptr;
      Value *out = nullptr;
    };

    void getAllocas(Function &F, std::vector<Value *> &allocas)
    {
      for (auto &bb : F)
      {
        for (auto &i : bb)
        {
          if (i.getOpcode() == Instruction::Alloca)
          {
            errs() << i << "\n";
            allocas.push_back(cast<Value>(&i));
          }
        }
      }
    }

    bool hasPathBetweenBlocks(BasicBlock *start, BasicBlock *end)
    {
      if (start == end)
      {
        return true;
      }

      for (auto child : successors(start))
      {
        if (hasPathBetweenBlocks(child, end))
        {
          return true;
        }
      }

      return false;
    }

    void findGenKillInOut(Function &F, Value *alloca, std::unordered_map<BasicBlock *, bbSinkingAnalysis> &analysis)
    {
      for (auto &bb : F)
      {
        bbSinkingAnalysis res;
        for (auto &i : bb)
        {
          if (i.getOpcode() == Instruction::Store && i.getOperand(1) == alloca)
          {
            res.kill = false;
            res.gen = &i;
          }
          if (i.getOpcode() == Instruction::Load && i.getOperand(0) == alloca)
          {
            res.gen = nullptr;
            res.kill = true;
          }
        }

        std::vector<BasicBlock *> successorsBB;
        for (auto successor : successors(&bb))
        {
          successorsBB.push_back(successor);
        }

        for (int i = 0; i < (int)(successorsBB.size()) - 1; ++i)
        {
          if (hasPathBetweenBlocks(successorsBB[i], successorsBB[i + 1]) || hasPathBetweenBlocks(successorsBB[i + 1], successorsBB[i]))
          {
            res.gen = nullptr;
            res.kill = true;
            break;
          }
        }

        analysis[&bb] = res;
      }

      bool change = true;
      while (change)
      {
        for (auto &bb : F)
        {
          auto &res = analysis[&bb];
          auto oldin = res.in;

          for (auto pred : predecessors(&bb))
          {
            auto out = analysis[pred].out;
            if (out == nullptr)
            {
              res.in = nullptr;
              break;
            }
            if (res.in == nullptr)
            {
              res.in = out;
              continue;
            }
            if (!dyn_cast<Instruction>(res.in)->isIdenticalTo(dyn_cast<Instruction>(out)))
            {
              res.in = nullptr;
              break;
            }
          }

          if (res.gen)
          {
            res.out = res.gen;
          }
          else if (!res.kill)
          {
            res.out = res.in;
          }

          change = oldin != res.in;
        }
      }
    }

    bool findPartialDeadStores(Function &F, Value *alloca, std::unordered_map<BasicBlock *, bbSinkingAnalysis> &analysis)
    {
      std::unordered_set<Instruction *> pds;
      for (auto &bb : F)
      {
        auto bbAnalysis = analysis[&bb];
        if (bbAnalysis.in != nullptr)
        {
          auto inst = dyn_cast<Instruction>(bbAnalysis.in)->clone();
          inst->insertBefore(&(*(bb.getFirstInsertionPt())));
          for (auto predBB : predecessors(&bb))
          {
            auto predBbAnalysis = analysis[predBB];
            pds.insert(dyn_cast<Instruction>(predBbAnalysis.out));
          }
        }
      }

      if (pds.empty())
      {
        return false;
      }

      for (auto i : pds)
      {
        i->eraseFromParent();
      }
      return true;
    }

    void rmDup(Function &F, Value *alloca)
    {
      for (auto &bb : F)
      {
        bool current_store = false;

        std::vector<Instruction *> toDel;

        for (auto i = bb.rbegin(); i != bb.rend(); ++i)
        {
          errs() << *i << "\n";
          if (i->getOpcode() == Instruction::Store && i->getOperand(1) == alloca)
          {
            if (!current_store)
            {
              current_store = true;
              continue;
            }
            toDel.push_back(&(*(i)));
          }
          else if (i->getOpcode() == Instruction::Load && i->getOperand(0) == alloca)
          {
            current_store = false;
          }
        }

        for (auto del : toDel)
        {
          del->eraseFromParent();
        }
      }
    }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
    {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
      llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
      /* *******Implementation Starts Here******* */
      // Your core logic should reside here.

      // Step 1: Find all variables allocated by Alloca instruction
      std::vector<Value *> allocas;
      getAllocas(F, allocas);

      // Step 2: Assignment sinking analysis

      for (auto alloca : allocas)
      {
        errs() << "alloca: " << *(cast<Instruction>(alloca)) << "\n";

        std::unordered_map<BasicBlock *, bbSinkingAnalysis> analysis;

        while (true)
        {
          findGenKillInOut(F, alloca, analysis);

          if (!findPartialDeadStores(F, alloca, analysis))
          {
            break;
          }
        }

        rmDup(F, alloca);

        for (auto i : analysis)
        {
          errs() << "BB: ";
          if (i.first != nullptr)
          {
            errs() << *i.first;
          }
          else
          {
            errs() << "null";
          }

          errs() << " GEN: ";
          if (i.second.gen != nullptr)
          {
            errs() << *i.second.gen;
          }
          else
          {
            errs() << "null";
          }

          errs() << " KILL: " << i.second.kill;

          errs() << " IN: ";
          if (i.second.in != nullptr)
          {
            errs() << *i.second.in;
          }
          else
          {
            errs() << "null";
          }

          errs() << " OUT: ";
          if (i.second.out != nullptr)
          {
            errs() << *i.second.out;
          }
          else
          {
            errs() << "null";
          }

          errs() << "\n";
        }
      }

      /* *******Implementation Ends Here******* */
      // Your pass is modifying the source code. Figure out which analyses
      // are preserved and only return those, not all.
      return PreservedAnalyses::all();
    }
  };
  struct PDSEPerformancePass : public PassInfoMixin<PDSEPerformancePass>
  {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
    {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
      llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
      /* *******Implementation Starts Here******* */
      // This is a bonus. You do not need to attempt this to receive full credit.
      /* *******Implementation Ends Here******* */

      // Your pass is modifying the source code. Figure out which analyses
      // are preserved and only return those, not all.
      return PreservedAnalyses::all();
    }
  };
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo()
{
  return {
      LLVM_PLUGIN_API_VERSION, "PDSEPass", "v0.1",
      [](PassBuilder &PB)
      {
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM,
               ArrayRef<PassBuilder::PipelineElement>)
            {
              if (Name == "pdse-correctness")
              {
                FPM.addPass(PDSECorrectnessPass());
                return true;
              }
              if (Name == "pdse-performance")
              {
                FPM.addPass(PDSEPerformancePass());
                return true;
              }
              return false;
            });
      }};
}
