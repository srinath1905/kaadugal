#ifndef _DECISIONFORESTBUILDER_HPP_
#define _DECISIONFORESTBUILDER_HPP_

#include <memory>

#include "DecisionForest.hpp"
#include "DecisionTreeBuilder.hpp"
#include "DataSetIndex.hpp"
#include "Randomizer.hpp"
#include "Utilities.hpp"

namespace Kaadugal
{
	// Data sampling enum
	enum TreeDataSamplingType
	{
		UniformPartition, // Split data uniformly between trees
		Constant, // All trees see all the data
		Bagging, // For data of size N, all trees get N samples but are chosen with replacement
	};

	// T: AbstractFeatureResponse which is the feature response function or weak learner
	// S: AbstractStatistics which contains some statistics about node from training
	// R: AbstractLeafData, arbitrary data stored if this is a leaf node
	template<class T, class S, class R = AbstractLeafData>
	class DecisionForestBuilder
	{
	private:
		std::shared_ptr<AbstractDataSet> m_DataSet;
		std::unique_ptr<DataSetIndex> m_DataSetIndex;
		std::vector<std::shared_ptr<DataSetIndex>> m_DataSubsetsIdx; // Each subset is passed to a tree for training
		const ForestBuilderParameters& m_Parameters; // Parameters also should never be modified
		std::vector<DecisionTreeBuilder<T, S, R>> m_TreeBuilders;
		DecisionForest<T, S, R> m_Forest;
		bool m_isForestTrained;

		uint64_t m_TimeStartedBuild;
		uint64_t m_TimeFinishedBuild;

		void RandomPartition(void)
		{
			int SetSize = m_DataSet->Size();
			// Create an indices set with all indices
			std::vector<int> Indices;
			for (int i = 0; i < SetSize; ++i)
				Indices.push_back(i);

			// Contains index to all points in the data set
			m_DataSetIndex = std::unique_ptr<DataSetIndex>(new DataSetIndex(m_DataSet, Indices));

			std::shuffle(Indices.begin(), Indices.end(), Randomizer::Get().GetRNG());
			// for(int i = 0; i < SetSize; ++i)
			// 	std::cout << Indices[i] << std::endl;

			Kaadugal::TreeDataSamplingType SamplType = TreeDataSamplingType::Bagging;

			if (SamplType == TreeDataSamplingType::UniformPartition)
			{
				std::cout << "[ INFO ]: Uniformly splitting data between trees." << std::endl;
				// OPTION 1: Evenly split the data among the trees
				int NumSubsets = m_Parameters.m_NumTrees;
				int SubsetSize = SetSize / NumSubsets;
				int Remainder = SetSize % NumSubsets;
				int RemCtr = 0;
				for (int i = 0; i < NumSubsets; ++i)
				{
					std::vector<int> SubIdx;
					for (int j = 0; j < SubsetSize; ++j)
						SubIdx.push_back(Indices[i*SubsetSize + j]);

					if (RemCtr != Remainder) // Let's distribute the remainder evenly to the first k (k = Remainder) trees
					{
						SubIdx.push_back(Indices[NumSubsets*SubsetSize + RemCtr]);
						RemCtr++;
					}

					// for(int i = 0; i < SubIdx.size(); ++i)
					//     std::cout << SubIdx[i] << std::endl;
					// std::cout << std::endl;

					m_DataSubsetsIdx.push_back(std::make_shared<DataSetIndex>(DataSetIndex(m_DataSet, SubIdx)));
				}
			}
			else if (SamplType == TreeDataSamplingType::Constant)
			{
				std::cout << "[ INFO ]: Passing all data to all trees." << std::endl;
				// OPTION 2: Pass all the data to all the trees and let randomness take care
				int NumSubsets = m_Parameters.m_NumTrees;
				for (int i = 0; i < NumSubsets; ++i)
				{
					std::vector<int> SubIdx = Indices;
					std::shuffle(SubIdx.begin(), SubIdx.end(), Randomizer::Get().GetRNG()); // Shuffle just to be sure

					m_DataSubsetsIdx.push_back(std::make_shared<DataSetIndex>(DataSetIndex(m_DataSet, SubIdx)));
				}
			}
			else if (SamplType == TreeDataSamplingType::Bagging)
			{
				std::cout << "[ INFO ]: Using bagging to split data between trees." << std::endl;
				// OPTION 3: Perform bagging with each tree getting SetSize bootstrap samples (i.e. sampled with replacement)
				int NumSubsets = m_Parameters.m_NumTrees;
				int SubsetSize = SetSize;
				for (int i = 0; i < NumSubsets; ++i)
				{
					std::vector<int> SubIdx;
					for (int j = 0; j < SubsetSize; ++j)
						SubIdx.push_back(Randomizer::GetRandomElement<int>(Indices)); // Random sampling *with* replacement

					m_DataSubsetsIdx.push_back(std::make_shared<DataSetIndex>(DataSetIndex(m_DataSet, SubIdx)));
				}
			}
		};

	public:
		DecisionForestBuilder(const ForestBuilderParameters& Parameters)
			: m_Parameters(Parameters)
			, m_isForestTrained(false)
		{
			for (int i = 0; i < m_Parameters.m_NumTrees; ++i)
				m_TreeBuilders.push_back(DecisionTreeBuilder<T, S, R>(m_Parameters));
		};

		bool Build(std::shared_ptr<AbstractDataSet> DataSet)
		{
			m_DataSet = DataSet;
			if (uint64_t(m_Parameters.m_NumTrees) > m_DataSet->Size())
			{
				std::cout << "[ WARN ]: The number of trees (" << m_Parameters.m_NumTrees << ") is greater than the number of training samples (" << m_DataSet->Size() << "). Cannot train forest." << std::endl;
				return false;
			}

			bool Success = true;

			RandomPartition(); // Randomly partition data set into NumTrees subsets

			int nTreeBuilders = m_TreeBuilders.size();
			m_TimeStartedBuild = GetCurrentEpochTime();
			for (int i = 0; i < nTreeBuilders; ++i)
			{
				std::cout << "[ INFO ]: Training tree number " << i << "..." << std::endl;
				bool TreeSuccess = m_TreeBuilders[i].Build(m_DataSubsetsIdx[i]);
				Success &= TreeSuccess;
				if (TreeSuccess)
					m_Forest.AddTree(m_TreeBuilders[i].GetTree());
				else
					std::cout << "[ ERROR ]: Problem training tree number " << i << "." << std::endl;
			}
			m_TimeFinishedBuild = GetCurrentEpochTime();
			std::cout << "[ INFO ]: Forest training took: " << (m_TimeFinishedBuild - m_TimeStartedBuild) * 1e-6 << " s." << std::endl;

			m_isForestTrained = Success;
			return m_isForestTrained;
		};

		DecisionForest<T, S, R>& GetForest(void) { return m_Forest; };
		bool DoneBuild(void) { return m_isForestTrained; };
	};
} // namespace Kaadugal

#endif // _DECISIONFORESTBUILDER_HPP_
