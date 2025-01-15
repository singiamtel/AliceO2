// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file GPUTPCTrackletConstructor.h
/// \author Sergey Gorbunov, David Rohr

#ifndef GPUTPCTRACKLETCONSTRUCTOR_H
#define GPUTPCTRACKLETCONSTRUCTOR_H

#include "GPUTPCDef.h"
#include "GPUTPCTrackParam.h"
#include "GPUGeneralKernels.h"
#include "GPUConstantMem.h"

namespace GPUCA_NAMESPACE
{
namespace gpu
{
/**
 * @class GPUTPCTrackletConstructor
 *
 */
class GPUTPCTracker;

class GPUTPCTrackletConstructor
{
 public:
  enum K {
    singleSlice = 0,
    allSlices = 1
  };

  class GPUTPCThreadMemory
  {
    friend class GPUTPCTrackletConstructor; //! friend class
   public:
#if !defined(GPUCA_GPUCODE)
    GPUTPCThreadMemory() : mISH(0), mFirstRow(0), mLastRow(0), mStartRow(0), mEndRow(0), mCurrIH(0), mGo(0), mStage(0), mNHits(0), mNHitsEndRow(0), mNMissed(0), mLastY(0), mLastZ(0)
    {
    }

    GPUTPCThreadMemory(const GPUTPCThreadMemory& /*dummy*/) : mISH(0), mFirstRow(0), mLastRow(0), mStartRow(0), mEndRow(0), mCurrIH(0), mGo(0), mStage(0), mNHits(0), mNHitsEndRow(0), mNMissed(0), mLastY(0), mLastZ(0) {}
    GPUTPCThreadMemory& operator=(const GPUTPCThreadMemory& /*dummy*/) { return *this; }
#endif //! GPUCA_GPUCODE

   protected:
    // WARNING: This data is copied element by element in CopyTrackletTempData. Changes to members of this class must be reflected in CopyTrackletTempData!!!
    int32_t mISH;      // track index
    int32_t mFirstRow; // first row index
    int32_t mLastRow;  // last row index
    int32_t mStartRow; // row index of first hit in seed
    int32_t mEndRow;   // row index of last hit in seed
    calink mCurrIH;   // indef of the current hit
    int8_t mGo;       // do fit/searching flag
    int32_t mStage;   // reco stage
    int32_t mNHits;   // n track hits
    int32_t mNHitsEndRow; // n hits at end row
    int32_t mNMissed;     // n missed hits during search
    float mLastY;     // Y of the last fitted cluster
    float mLastZ;     // Z of the last fitted cluster
  };

  struct GPUSharedMemory {
    CA_SHARED_STORAGE(GPUTPCRow mRows[GPUCA_ROW_COUNT]);         // rows
    int32_t mNextStartHitFirst;                                  // First start hit to be processed by CUDA block during next iteration
    int32_t mNextStartHitCount;                                  // Number of start hits to be processed by CUDA block during next iteration
    int32_t mNextStartHitFirstRun;                               // First run for dynamic scheduler?
    int32_t mNStartHits;                                         // Total number of start hits

#ifdef GPUCA_TRACKLET_CONSTRUCTOR_DO_PROFILE
    int32_t fMaxSync; // temporary shared variable during profile creation
#endif            // GPUCA_TRACKLET_CONSTRUCTOR_DO_PROFILE
  };

  GPUd() static void InitTracklet(GPUTPCTrackParam& tParam);

  template <class T>
  GPUd() static void UpdateTracklet(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() T& s, GPUTPCThreadMemory& r, GPUconstantref() GPUTPCTracker& tracker, GPUTPCTrackParam& tParam, int32_t iRow, calink& rowHit, calink* rowHits);

  GPUd() static void StoreTracklet(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& s, GPUTPCThreadMemory& r, GPUconstantref() GPUTPCTracker& tracker, GPUTPCTrackParam& tParam, calink* rowHits);

  GPUd() static bool CheckCov(GPUTPCTrackParam& tParam);

  GPUd() static void DoTracklet(GPUconstantref() GPUTPCTracker& tracker, GPUsharedref() GPUTPCTrackletConstructor::GPUSharedMemory& sMem, GPUTPCThreadMemory& rMem);

#ifdef GPUCA_GPUCODE
  GPUd() static int32_t FetchTracklet(GPUconstantref() GPUTPCTracker& tracker, GPUsharedref() GPUSharedMemory& sMem);
#endif // GPUCA_GPUCODE

  template <class T>
  GPUd() static int32_t GPUTPCTrackletConstructorGlobalTracking(GPUconstantref() GPUTPCTracker& tracker, GPUsharedref() T& sMem, GPUTPCTrackParam& tParam, int32_t startrow, int32_t increment, int32_t iTracklet, calink* rowHits);

  typedef GPUconstantref() GPUTPCTracker processorType;
  GPUhdi() CONSTEXPR static GPUDataTypes::RecoStep GetRecoStep() { return GPUCA_RECO_STEP::TPCSliceTracking; }
  GPUhdi() static processorType* Processor(GPUConstantMem& processors)
  {
    return processors.tpcTrackers;
  }
  template <int32_t iKernel = GPUKernelTemplate::defaultKernel>
  GPUd() static void Thread(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUsharedref() GPUSharedMemory& smem, processorType& tracker);
};

} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif // GPUTPCTRACKLETCONSTRUCTOR_H
