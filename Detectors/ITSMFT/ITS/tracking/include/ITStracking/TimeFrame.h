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
///

#ifndef TRACKINGITSU_INCLUDE_TIMEFRAME_H_
#define TRACKINGITSU_INCLUDE_TIMEFRAME_H_

#include <array>
#include <vector>
#include <utility>
#include <numeric>
#include <cassert>
#include <gsl/gsl>
#include <numeric>
#include <iostream>
#include <algorithm>

#include "DataFormatsITS/TrackITS.h"

#include "ITStracking/Cell.h"
#include "ITStracking/Cluster.h"
#include "ITStracking/Configuration.h"
#include "ITStracking/Constants.h"
#include "ITStracking/ClusterLines.h"
#include "ITStracking/Definitions.h"
#include "ITStracking/Road.h"
#include "ITStracking/Tracklet.h"
#include "ITStracking/IndexTableUtils.h"
#include "ITStracking/ExternalAllocator.h"

#include "SimulationDataFormat/MCCompLabel.h"
#include "SimulationDataFormat/MCTruthContainer.h"

#include "ReconstructionDataFormats/Vertex.h"
#include "DetectorsBase/Propagator.h"

namespace o2
{
namespace gpu
{
class GPUChainITS;
}

namespace itsmft
{
class Cluster;
class CompClusterExt;
class TopologyDictionary;
class ROFRecord;
} // namespace itsmft

namespace its
{
using Vertex = o2::dataformats::Vertex<o2::dataformats::TimeStamp<int>>;

class TimeFrame
{
 public:
  friend class TimeFrameGPU;
  TimeFrame(int nLayers = 7);
  const Vertex& getPrimaryVertex(const int) const;
  gsl::span<const Vertex> getPrimaryVertices(int rofId) const;
  gsl::span<const Vertex> getPrimaryVertices(int romin, int romax) const;
  gsl::span<const std::pair<MCCompLabel, float>> getPrimaryVerticesMCRecInfo(const int rofId) const;
  gsl::span<const std::array<float, 2>> getPrimaryVerticesXAlpha(int rofId) const;
  void fillPrimaryVerticesXandAlpha();
  int getPrimaryVerticesNum(int rofId = -1) const;
  void addPrimaryVertices(const std::vector<Vertex>& vertices);
  void addPrimaryVerticesLabels(std::vector<std::pair<MCCompLabel, float>>& labels);
  void addPrimaryVertices(const std::vector<Vertex>& vertices, const int rofId, const int iteration);
  void addPrimaryVertices(const gsl::span<const Vertex>& vertices, const int rofId, const int iteration);
  void addPrimaryVerticesInROF(const std::vector<Vertex>& vertices, const int rofId, const int iteration);
  void addPrimaryVerticesLabelsInROF(const std::vector<std::pair<MCCompLabel, float>>& labels, const int rofId);
  void removePrimaryVerticesInROf(const int rofId);
  int loadROFrameData(const o2::itsmft::ROFRecord& rof, gsl::span<const itsmft::Cluster> clusters,
                      const dataformats::MCTruthContainer<MCCompLabel>* mcLabels = nullptr);

  int loadROFrameData(gsl::span<o2::itsmft::ROFRecord> rofs,
                      gsl::span<const itsmft::CompClusterExt> clusters,
                      gsl::span<const unsigned char>::iterator& pattIt,
                      const itsmft::TopologyDictionary* dict,
                      const dataformats::MCTruthContainer<MCCompLabel>* mcLabels = nullptr);

  int getTotalClusters() const;
  std::vector<int>& getTotVertIteration() { return mTotVertPerIteration; }
  bool empty() const;
  bool isGPU() const { return mIsGPU; }
  int getSortedIndex(int rofId, int layer, int i) const;
  int getSortedStartIndex(const int, const int) const;
  int getNrof() const;

  void resetBeamXY(const float x, const float y, const float w = 0);
  void setBeamPosition(const float x, const float y, const float s2, const float base = 50.f, const float systematic = 0.f)
  {
    isBeamPositionOverridden = true;
    resetBeamXY(x, y, s2 / o2::gpu::CAMath::Sqrt(base * base + systematic));
  }

  float getBeamX() const;
  float getBeamY() const;
  std::vector<float>& getMinRs() { return mMinR; }
  std::vector<float>& getMaxRs() { return mMaxR; }
  float getMinR(int layer) const { return mMinR[layer]; }
  float getMaxR(int layer) const { return mMaxR[layer]; }
  float getMSangle(int layer) const { return mMSangles[layer]; }
  std::vector<float>& getMSangles() { return mMSangles; }
  float getPhiCut(int layer) const { return mPhiCuts[layer]; }
  std::vector<float>& getPhiCuts() { return mPhiCuts; }
  float getPositionResolution(int layer) const { return mPositionResolution[layer]; }
  std::vector<float>& getPositionResolutions() { return mPositionResolution; }

  gsl::span<Cluster> getClustersOnLayer(int rofId, int layerId);
  gsl::span<const Cluster> getClustersOnLayer(int rofId, int layerId) const;
  gsl::span<const Cluster> getClustersPerROFrange(int rofMin, int range, int layerId) const;
  gsl::span<const Cluster> getUnsortedClustersOnLayer(int rofId, int layerId) const;
  gsl::span<unsigned char> getUsedClustersROF(int rofId, int layerId);
  gsl::span<const unsigned char> getUsedClustersROF(int rofId, int layerId) const;
  gsl::span<const int> getROFramesClustersPerROFrange(int rofMin, int range, int layerId) const;
  gsl::span<const int> getROFrameClusters(int layerId) const;
  gsl::span<const int> getNClustersROFrange(int rofMin, int range, int layerId) const;
  gsl::span<const int> getIndexTablePerROFrange(int rofMin, int range, int layerId) const;
  gsl::span<int> getIndexTable(int rofId, int layerId);
  std::vector<int>& getIndexTableWhole(int layerId) { return mIndexTables[layerId]; }
  const std::vector<TrackingFrameInfo>& getTrackingFrameInfoOnLayer(int layerId) const;

  const TrackingFrameInfo& getClusterTrackingFrameInfo(int layerId, const Cluster& cl) const;
  const gsl::span<const MCCompLabel> getClusterLabels(int layerId, const Cluster& cl) const;
  const gsl::span<const MCCompLabel> getClusterLabels(int layerId, const int clId) const;
  int getClusterExternalIndex(int layerId, const int clId) const;
  int getClusterSize(int clusterId) const;
  void setClusterSize(const std::vector<uint8_t>& v) { mClusterSize = v; };

  std::vector<MCCompLabel>& getTrackletsLabel(int layer) { return mTrackletLabels[layer]; }
  std::vector<MCCompLabel>& getCellsLabel(int layer) { return mCellLabels[layer]; }

  bool hasMCinformation() const;
  void initialise(const int iteration, const TrackingParameters& trkParam, const int maxLayers = 7, bool resetVertices = true);
  void resetRofPV()
  {
    deepVectorClear(mPrimaryVertices);
    mROFramesPV.resize(1, 0);
    mTotVertPerIteration.resize(1);
  };

  bool isClusterUsed(int layer, int clusterId) const;
  void markUsedCluster(int layer, int clusterId);
  gsl::span<unsigned char> getUsedClusters(const int layer);

  std::vector<std::vector<Tracklet>>& getTracklets();
  std::vector<std::vector<int>>& getTrackletsLookupTable();

  std::vector<std::vector<Cluster>>& getClusters();
  std::vector<std::vector<Cluster>>& getUnsortedClusters();
  int getClusterROF(int iLayer, int iCluster);
  std::vector<std::vector<CellSeed>>& getCells();

  std::vector<std::vector<int>>& getCellsLookupTable();
  std::vector<std::vector<int>>& getCellsNeighbours();
  std::vector<std::vector<int>>& getCellsNeighboursLUT();
  std::vector<Road<5>>& getRoads();
  std::vector<TrackITSExt>& getTracks(int rofId) { return mTracks[rofId]; }
  std::vector<MCCompLabel>& getTracksLabel(const int rofId) { return mTracksLabel[rofId]; }
  std::vector<MCCompLabel>& getLinesLabel(const int rofId) { return mLinesLabels[rofId]; }
  std::vector<std::pair<MCCompLabel, float>>& getVerticesMCRecInfo() { return mVerticesMCRecInfo; }

  int getNumberOfClusters() const;
  int getNumberOfCells() const;
  int getNumberOfTracklets() const;
  int getNumberOfNeighbours() const;
  size_t getNumberOfTracks() const;
  size_t getNumberOfUsedClusters() const;
  auto getNumberOfExtendedTracks() const { return mNExtendedTracks; }
  auto getNumberOfUsedExtendedClusters() const { return mNExtendedUsedClusters; }

  bool checkMemory(unsigned long max) { return getArtefactsMemory() < max; }
  unsigned long getArtefactsMemory();
  int getROFCutClusterMult() const { return mCutClusterMult; };
  int getROFCutVertexMult() const { return mCutVertexMult; };
  int getROFCutAllMult() const { return mCutClusterMult + mCutVertexMult; }

  // Vertexer
  void computeTrackletsPerROFScans();
  void computeTracletsPerClusterScans();
  int& getNTrackletsROF(int rofId, int combId);
  std::vector<Line>& getLines(int rofId);
  int getNLinesTotal() const
  {
    return std::accumulate(mLines.begin(), mLines.end(), 0, [](int sum, const auto& l) { return sum + l.size(); });
  }
  std::vector<ClusterLines>& getTrackletClusters(int rofId);
  gsl::span<const Tracklet> getFoundTracklets(int rofId, int combId) const;
  gsl::span<Tracklet> getFoundTracklets(int rofId, int combId);
  gsl::span<const MCCompLabel> getLabelsFoundTracklets(int rofId, int combId) const;
  gsl::span<int> getNTrackletsCluster(int rofId, int combId);
  gsl::span<int> getExclusiveNTrackletsCluster(int rofId, int combId);
  uint32_t getTotalTrackletsTF(const int iLayer) { return mTotalTracklets[iLayer]; }
  int getTotalClustersPerROFrange(int rofMin, int range, int layerId) const;
  std::array<float, 2>& getBeamXY() { return mBeamPos; }
  unsigned int& getNoVertexROF() { return mNoVertexROF; }
  void insertPastVertex(const Vertex& vertex, const int refROFId);
  // \Vertexer

  void initialiseRoadLabels();
  void setRoadLabel(int i, const unsigned long long& lab, bool fake);
  const unsigned long long& getRoadLabel(int i) const;
  bool isRoadFake(int i) const;

  void setMultiplicityCutMask(const std::vector<uint8_t>& cutMask) { mMultiplicityCutMask = cutMask; }
  void setROFMask(const std::vector<uint8_t>& rofMask) { mROFMask = rofMask; }
  void swapMasks() { mMultiplicityCutMask.swap(mROFMask); }

  int hasBogusClusters() const { return std::accumulate(mBogusClusters.begin(), mBogusClusters.end(), 0); }

  void setBz(float bz) { mBz = bz; }
  float getBz() const { return mBz; }

  void setExternalAllocator(ExternalAllocator* allocator)
  {
    if (mIsGPU) {
      LOGP(debug, "Setting timeFrame allocator to external");
      mAllocator = allocator;
      mExtAllocator = true; // to be removed
    } else {
      LOGP(debug, "External allocator is currently only supported for GPU");
    }
  }

  virtual void setDevicePropagator(const o2::base::PropagatorImpl<float>*)
  {
    return;
  };
  const o2::base::PropagatorImpl<float>* getDevicePropagator() const { return mPropagatorDevice; }

  template <typename... T>
  void addClusterToLayer(int layer, T&&... args);
  template <typename... T>
  void addTrackingFrameInfoToLayer(int layer, T&&... args);
  void addClusterExternalIndexToLayer(int layer, const int idx);

  void resizeVectors(int nLayers);

  void setExtAllocator(bool ext) { mExtAllocator = ext; }
  bool getExtAllocator() const { return mExtAllocator; }

  /// Debug and printing
  void checkTrackletLUTs();
  void printROFoffsets();
  void printNClsPerROF();
  void printVertices();
  void printTrackletLUTonLayer(int i);
  void printCellLUTonLayer(int i);
  void printTrackletLUTs();
  void printCellLUTs();
  void printSliceInfo(const int, const int);

  IndexTableUtils mIndexTableUtils;

  bool mIsGPU = false;

  std::vector<std::vector<Cluster>> mClusters;
  std::vector<std::vector<TrackingFrameInfo>> mTrackingFrameInfo;
  std::vector<std::vector<int>> mClusterExternalIndices;
  std::vector<std::vector<int>> mROFramesClusters;
  const dataformats::MCTruthContainer<MCCompLabel>* mClusterLabels = nullptr;
  std::array<std::vector<int>, 2> mNTrackletsPerCluster;
  std::array<std::vector<int>, 2> mNTrackletsPerClusterSum;
  std::vector<std::vector<int>> mNClustersPerROF;
  std::vector<std::vector<int>> mIndexTables;
  std::vector<std::vector<int>> mTrackletsLookupTable;
  std::vector<std::vector<unsigned char>> mUsedClusters;
  int mNrof = 0;
  int mNExtendedTracks{0};
  int mNExtendedUsedClusters{0};
  std::vector<int> mROFramesPV = {0};
  std::vector<Vertex> mPrimaryVertices;

  // State if memory will be externally managed.
  bool mExtAllocator = false;
  ExternalAllocator* mAllocator = nullptr;
  std::vector<std::vector<Cluster>> mUnsortedClusters;
  std::vector<std::vector<Tracklet>> mTracklets;
  std::vector<std::vector<CellSeed>> mCells;
  std::vector<std::vector<o2::track::TrackParCovF>> mCellSeeds;
  std::vector<std::vector<float>> mCellSeedsChi2;
  std::vector<Road<5>> mRoads;
  std::vector<std::vector<TrackITSExt>> mTracks;
  std::vector<std::vector<int>> mCellsNeighbours;
  std::vector<std::vector<int>> mCellsLookupTable;
  std::vector<uint8_t> mMultiplicityCutMask;

  const o2::base::PropagatorImpl<float>* mPropagatorDevice = nullptr; // Needed only for GPU
  void dropTracks()
  {
    for (auto& v : mTracks) {
      deepVectorClear(v);
    }
  }

 protected:
  template <typename T>
  void deepVectorClear(std::vector<T>& vec)
  {
    std::vector<T>().swap(vec);
  }

 private:
  void prepareClusters(const TrackingParameters& trkParam, const int maxLayers);
  float mBz = 5.;
  unsigned int mNTotalLowPtVertices = 0;
  int mBeamPosWeight = 0;
  std::array<float, 2> mBeamPos = {0.f, 0.f};
  bool isBeamPositionOverridden = false;
  std::vector<float> mMinR;
  std::vector<float> mMaxR;
  std::vector<float> mMSangles;
  std::vector<float> mPhiCuts;
  std::vector<float> mPositionResolution;
  std::vector<uint8_t> mClusterSize;

  std::vector<uint8_t> mROFMask;
  std::vector<std::array<float, 2>> mPValphaX; /// PV x and alpha for track propagation
  std::vector<std::vector<MCCompLabel>> mTrackletLabels;
  std::vector<std::vector<MCCompLabel>> mCellLabels;
  std::vector<std::vector<int>> mCellsNeighboursLUT;
  std::vector<std::vector<MCCompLabel>> mTracksLabel;
  std::vector<int> mBogusClusters; /// keep track of clusters with wild coordinates

  std::vector<std::pair<unsigned long long, bool>> mRoadLabels;
  int mCutClusterMult;
  int mCutVertexMult;

  // Vertexer
  std::vector<std::vector<int>> mNTrackletsPerROF;
  std::vector<std::vector<Line>> mLines;
  std::vector<std::vector<ClusterLines>> mTrackletClusters;
  std::vector<std::vector<int>> mTrackletsIndexROF;
  std::vector<std::vector<MCCompLabel>> mLinesLabels;
  std::vector<std::pair<MCCompLabel, float>> mVerticesMCRecInfo;
  std::array<uint32_t, 2> mTotalTracklets = {0, 0};
  unsigned int mNoVertexROF = 0;
  std::vector<int> mTotVertPerIteration;
  // \Vertexer
};

inline const Vertex& TimeFrame::getPrimaryVertex(const int vertexIndex) const { return mPrimaryVertices[vertexIndex]; }

inline gsl::span<const Vertex> TimeFrame::getPrimaryVertices(int rofId) const
{
  const int start = mROFramesPV[rofId];
  const int stop_idx = rofId >= mNrof - 1 ? mNrof : rofId + 1;
  int delta = mMultiplicityCutMask[rofId] ? mROFramesPV[stop_idx] - start : 0; // return empty span if Rof is excluded
  return {&mPrimaryVertices[start], static_cast<gsl::span<const Vertex>::size_type>(delta)};
}

inline gsl::span<const std::pair<MCCompLabel, float>> TimeFrame::getPrimaryVerticesMCRecInfo(const int rofId) const
{
  const int start = mROFramesPV[rofId];
  const int stop_idx = rofId >= mNrof - 1 ? mNrof : rofId + 1;
  int delta = mMultiplicityCutMask[rofId] ? mROFramesPV[stop_idx] - start : 0; // return empty span if Rof is excluded
  return {&(mVerticesMCRecInfo[start]), static_cast<gsl::span<const std::pair<MCCompLabel, float>>::size_type>(delta)};
}

inline gsl::span<const Vertex> TimeFrame::getPrimaryVertices(int romin, int romax) const
{
  return {&mPrimaryVertices[mROFramesPV[romin]], static_cast<gsl::span<const Vertex>::size_type>(mROFramesPV[romax + 1] - mROFramesPV[romin])};
}

inline gsl::span<const std::array<float, 2>> TimeFrame::getPrimaryVerticesXAlpha(int rofId) const
{
  const int start = mROFramesPV[rofId];
  const int stop_idx = rofId >= mNrof - 1 ? mNrof : rofId + 1;
  int delta = mMultiplicityCutMask[rofId] ? mROFramesPV[stop_idx] - start : 0; // return empty span if Rof is excluded
  return {&(mPValphaX[start]), static_cast<gsl::span<const std::array<float, 2>>::size_type>(delta)};
}

inline int TimeFrame::getPrimaryVerticesNum(int rofId) const
{
  return rofId < 0 ? mPrimaryVertices.size() : mROFramesPV[rofId + 1] - mROFramesPV[rofId];
}

inline bool TimeFrame::empty() const { return getTotalClusters() == 0; }

inline int TimeFrame::getSortedIndex(int rofId, int layer, int index) const { return mROFramesClusters[layer][rofId] + index; }

inline int TimeFrame::getSortedStartIndex(const int rofId, const int layer) const { return mROFramesClusters[layer][rofId]; }

inline int TimeFrame::getNrof() const { return mNrof; }

inline void TimeFrame::resetBeamXY(const float x, const float y, const float w)
{
  mBeamPos[0] = x;
  mBeamPos[1] = y;
  mBeamPosWeight = w;
}

inline float TimeFrame::getBeamX() const { return mBeamPos[0]; }

inline float TimeFrame::getBeamY() const { return mBeamPos[1]; }

inline gsl::span<const int> TimeFrame::getROFrameClusters(int layerId) const
{
  return {&mROFramesClusters[layerId][0], static_cast<gsl::span<const int>::size_type>(mROFramesClusters[layerId].size())};
}

inline gsl::span<Cluster> TimeFrame::getClustersOnLayer(int rofId, int layerId)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<Cluster>();
  }
  int startIdx{mROFramesClusters[layerId][rofId]};
  return {&mClusters[layerId][startIdx], static_cast<gsl::span<Cluster>::size_type>(mROFramesClusters[layerId][rofId + 1] - startIdx)};
}

inline gsl::span<const Cluster> TimeFrame::getClustersOnLayer(int rofId, int layerId) const
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<const Cluster>();
  }
  int startIdx{mROFramesClusters[layerId][rofId]};
  return {&mClusters[layerId][startIdx], static_cast<gsl::span<Cluster>::size_type>(mROFramesClusters[layerId][rofId + 1] - startIdx)};
}

inline gsl::span<unsigned char> TimeFrame::getUsedClustersROF(int rofId, int layerId)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<unsigned char>();
  }
  int startIdx{mROFramesClusters[layerId][rofId]};
  return {&mUsedClusters[layerId][startIdx], static_cast<gsl::span<unsigned char>::size_type>(mROFramesClusters[layerId][rofId + 1] - startIdx)};
}

inline gsl::span<const unsigned char> TimeFrame::getUsedClustersROF(int rofId, int layerId) const
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<const unsigned char>();
  }
  int startIdx{mROFramesClusters[layerId][rofId]};
  return {&mUsedClusters[layerId][startIdx], static_cast<gsl::span<unsigned char>::size_type>(mROFramesClusters[layerId][rofId + 1] - startIdx)};
}

inline gsl::span<const Cluster> TimeFrame::getClustersPerROFrange(int rofMin, int range, int layerId) const
{
  if (rofMin < 0 || rofMin >= mNrof) {
    return gsl::span<const Cluster>();
  }
  int startIdx{mROFramesClusters[layerId][rofMin]}; // First cluster of rofMin
  int endIdx{mROFramesClusters[layerId][o2::gpu::CAMath::Min(rofMin + range, mNrof)]};
  return {&mClusters[layerId][startIdx], static_cast<gsl::span<Cluster>::size_type>(endIdx - startIdx)};
}

inline gsl::span<const int> TimeFrame::getROFramesClustersPerROFrange(int rofMin, int range, int layerId) const
{
  int chkdRange{o2::gpu::CAMath::Min(range, mNrof - rofMin)};
  return {&mROFramesClusters[layerId][rofMin], static_cast<gsl::span<int>::size_type>(chkdRange)};
}

inline gsl::span<const int> TimeFrame::getNClustersROFrange(int rofMin, int range, int layerId) const
{
  int chkdRange{o2::gpu::CAMath::Min(range, mNrof - rofMin)};
  return {&mNClustersPerROF[layerId][rofMin], static_cast<gsl::span<int>::size_type>(chkdRange)};
}

inline int TimeFrame::getTotalClustersPerROFrange(int rofMin, int range, int layerId) const
{
  int startIdx{rofMin}; // First cluster of rofMin
  int endIdx{o2::gpu::CAMath::Min(rofMin + range, mNrof)};
  return mROFramesClusters[layerId][endIdx] - mROFramesClusters[layerId][startIdx];
}

inline gsl::span<const int> TimeFrame::getIndexTablePerROFrange(int rofMin, int range, int layerId) const
{
  const int iTableSize{mIndexTableUtils.getNphiBins() * mIndexTableUtils.getNzBins() + 1};
  int chkdRange{o2::gpu::CAMath::Min(range, mNrof - rofMin)};
  return {&mIndexTables[layerId][rofMin * iTableSize], static_cast<gsl::span<int>::size_type>(chkdRange * iTableSize)};
}

inline int TimeFrame::getClusterROF(int iLayer, int iCluster)
{
  return std::lower_bound(mROFramesClusters[iLayer].begin(), mROFramesClusters[iLayer].end(), iCluster + 1) - mROFramesClusters[iLayer].begin() - 1;
}

inline gsl::span<const Cluster> TimeFrame::getUnsortedClustersOnLayer(int rofId, int layerId) const
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<const Cluster>();
  }
  int startIdx{mROFramesClusters[layerId][rofId]};
  return {&mUnsortedClusters[layerId][startIdx], static_cast<gsl::span<Cluster>::size_type>(mROFramesClusters[layerId][rofId + 1] - startIdx)};
}

inline const std::vector<TrackingFrameInfo>& TimeFrame::getTrackingFrameInfoOnLayer(int layerId) const
{
  return mTrackingFrameInfo[layerId];
}

inline const TrackingFrameInfo& TimeFrame::getClusterTrackingFrameInfo(int layerId, const Cluster& cl) const
{
  return mTrackingFrameInfo[layerId][cl.clusterId];
}

inline const gsl::span<const MCCompLabel> TimeFrame::getClusterLabels(int layerId, const Cluster& cl) const
{
  return getClusterLabels(layerId, cl.clusterId);
}

inline const gsl::span<const MCCompLabel> TimeFrame::getClusterLabels(int layerId, int clId) const
{
  return mClusterLabels->getLabels(mClusterExternalIndices[layerId][clId]);
}

inline int TimeFrame::getClusterSize(int clusterId) const
{
  return mClusterSize[clusterId];
}

inline int TimeFrame::getClusterExternalIndex(int layerId, const int clId) const
{
  return mClusterExternalIndices[layerId][clId];
}

inline gsl::span<int> TimeFrame::getIndexTable(int rofId, int layer)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<int>();
  }
  return {&mIndexTables[layer][rofId * (mIndexTableUtils.getNphiBins() * mIndexTableUtils.getNzBins() + 1)],
          static_cast<gsl::span<int>::size_type>(mIndexTableUtils.getNphiBins() * mIndexTableUtils.getNzBins() + 1)};
}

inline std::vector<Line>& TimeFrame::getLines(int rofId)
{
  return mLines[rofId];
}

inline std::vector<ClusterLines>& TimeFrame::getTrackletClusters(int rofId)
{
  return mTrackletClusters[rofId];
}

template <typename... T>
void TimeFrame::addClusterToLayer(int layer, T&&... values)
{
  mUnsortedClusters[layer].emplace_back(std::forward<T>(values)...);
}

template <typename... T>
void TimeFrame::addTrackingFrameInfoToLayer(int layer, T&&... values)
{
  mTrackingFrameInfo[layer].emplace_back(std::forward<T>(values)...);
}

inline void TimeFrame::addClusterExternalIndexToLayer(int layer, const int idx)
{
  mClusterExternalIndices[layer].push_back(idx);
}

inline bool TimeFrame::hasMCinformation() const
{
  return mClusterLabels;
}

inline bool TimeFrame::isClusterUsed(int layer, int clusterId) const
{
  return mUsedClusters[layer][clusterId];
}

inline gsl::span<unsigned char> TimeFrame::getUsedClusters(const int layer)
{
  return {&mUsedClusters[layer][0], static_cast<gsl::span<unsigned char>::size_type>(mUsedClusters[layer].size())};
}

inline void TimeFrame::markUsedCluster(int layer, int clusterId) { mUsedClusters[layer][clusterId] = true; }

inline std::vector<std::vector<Tracklet>>& TimeFrame::getTracklets()
{
  return mTracklets;
}

inline std::vector<std::vector<int>>& TimeFrame::getTrackletsLookupTable()
{
  return mTrackletsLookupTable;
}

inline void TimeFrame::initialiseRoadLabels()
{
  mRoadLabels.clear();
  mRoadLabels.resize(mRoads.size());
}

inline void TimeFrame::setRoadLabel(int i, const unsigned long long& lab, bool fake)
{
  mRoadLabels[i].first = lab;
  mRoadLabels[i].second = fake;
}

inline const unsigned long long& TimeFrame::getRoadLabel(int i) const
{
  return mRoadLabels[i].first;
}

inline gsl::span<int> TimeFrame::getNTrackletsCluster(int rofId, int combId)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<int>();
  }
  auto startIdx{mROFramesClusters[1][rofId]};
  return {&mNTrackletsPerCluster[combId][startIdx], static_cast<gsl::span<int>::size_type>(mROFramesClusters[1][rofId + 1] - startIdx)};
}

inline gsl::span<int> TimeFrame::getExclusiveNTrackletsCluster(int rofId, int combId)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<int>();
  }
  auto clusStartIdx{mROFramesClusters[1][rofId]};

  return {&mNTrackletsPerClusterSum[combId][clusStartIdx], static_cast<gsl::span<int>::size_type>(mROFramesClusters[1][rofId + 1] - clusStartIdx)};
}

inline int& TimeFrame::getNTrackletsROF(int rofId, int combId)
{
  return mNTrackletsPerROF[combId][rofId];
}

inline bool TimeFrame::isRoadFake(int i) const
{
  return mRoadLabels[i].second;
}

inline std::vector<std::vector<Cluster>>& TimeFrame::getClusters()
{
  return mClusters;
}

inline std::vector<std::vector<Cluster>>& TimeFrame::getUnsortedClusters()
{
  return mUnsortedClusters;
}

inline std::vector<std::vector<CellSeed>>& TimeFrame::getCells() { return mCells; }

inline std::vector<std::vector<int>>& TimeFrame::getCellsLookupTable()
{
  return mCellsLookupTable;
}

inline std::vector<std::vector<int>>& TimeFrame::getCellsNeighbours() { return mCellsNeighbours; }
inline std::vector<std::vector<int>>& TimeFrame::getCellsNeighboursLUT() { return mCellsNeighboursLUT; }

inline std::vector<Road<5>>& TimeFrame::getRoads() { return mRoads; }

inline gsl::span<Tracklet> TimeFrame::getFoundTracklets(int rofId, int combId)
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<Tracklet>();
  }
  auto startIdx{mNTrackletsPerROF[combId][rofId]};
  return {&mTracklets[combId][startIdx], static_cast<gsl::span<Tracklet>::size_type>(mNTrackletsPerROF[combId][rofId + 1] - startIdx)};
}

inline gsl::span<const Tracklet> TimeFrame::getFoundTracklets(int rofId, int combId) const
{
  if (rofId < 0 || rofId >= mNrof) {
    return gsl::span<const Tracklet>();
  }
  auto startIdx{mNTrackletsPerROF[combId][rofId]};
  return {&mTracklets[combId][startIdx], static_cast<gsl::span<Tracklet>::size_type>(mNTrackletsPerROF[combId][rofId + 1] - startIdx)};
}

inline gsl::span<const MCCompLabel> TimeFrame::getLabelsFoundTracklets(int rofId, int combId) const
{
  if (rofId < 0 || rofId >= mNrof || !hasMCinformation()) {
    return gsl::span<const MCCompLabel>();
  }
  auto startIdx{mNTrackletsPerROF[combId][rofId]};
  return {&mTrackletLabels[combId][startIdx], static_cast<gsl::span<Tracklet>::size_type>(mNTrackletsPerROF[combId][rofId + 1] - startIdx)};
}

inline int TimeFrame::getNumberOfClusters() const
{
  int nClusters = 0;
  for (auto& layer : mClusters) {
    nClusters += layer.size();
  }
  return nClusters;
}

inline int TimeFrame::getNumberOfCells() const
{
  int nCells = 0;
  for (auto& layer : mCells) {
    nCells += layer.size();
  }
  return nCells;
}

inline int TimeFrame::getNumberOfTracklets() const
{
  int nTracklets = 0;
  for (auto& layer : mTracklets) {
    nTracklets += layer.size();
  }
  return nTracklets;
}

inline int TimeFrame::getNumberOfNeighbours() const
{
  int n{0};
  for (auto& l : mCellsNeighbours) {
    n += l.size();
  }
  return n;
}

inline size_t TimeFrame::getNumberOfTracks() const
{
  int nTracks = 0;
  for (auto& t : mTracks) {
    nTracks += t.size();
  }
  return nTracks;
}

inline size_t TimeFrame::getNumberOfUsedClusters() const
{
  size_t nClusters = 0;
  for (auto& layer : mUsedClusters) {
    nClusters += std::count(layer.begin(), layer.end(), true);
  }
  return nClusters;
}

inline void TimeFrame::insertPastVertex(const Vertex& vertex, const int iteration)
{
  int rofId = vertex.getTimeStamp().getTimeStamp();
  mPrimaryVertices.insert(mPrimaryVertices.begin() + mROFramesPV[rofId], vertex);
  for (int i = rofId + 1; i < mROFramesPV.size(); ++i) {
    mROFramesPV[i]++;
  }
  mTotVertPerIteration[iteration]++;
}

} // namespace its
} // namespace o2

#endif
