#include <deque>
#include <ctime>
#include <memory>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <unordered_set>

#include "lib/SpookyV2.h"
#include "vertexenumerator.h"

namespace Sibelia
{	
	const size_t VertexEnumerator::INVALID_VERTEX = -1;

	namespace
	{
		void PutInBloomFilter(std::vector<bool> & bitVector, const std::vector<std::pair<uint64_t, SpookyHash> > & hash, const DnaString & item)
		{
			for (const std::pair<uint64_t, SpookyHash> & h : hash)
			{
				uint64_t body = item.GetBody();
				uint64_t hvalue = h.second.Hash64(&body, sizeof(body), h.first);
				bitVector[hvalue % bitVector.size()] = true;
			}
		}

		bool IsInBloomFilter(const std::vector<bool> & bitVector, const std::vector<std::pair<uint64_t, SpookyHash> > & hash, const DnaString & item)
		{
			for (const std::pair<uint64_t, SpookyHash> & h : hash)
			{
				uint64_t body = item.GetBody();
				uint64_t hvalue = h.second.Hash64(&body, sizeof(body), h.first);
				if (!bitVector[hvalue % bitVector.size()])
				{
					return false;
				}
			}

			return true;
		}

		
	}

	VertexEnumerator::VertexEnumerator(const std::vector<std::string> & fileName, size_t vertexLength, size_t filterSize)
	{
		if (vertexLength > 29)
		{
			throw std::runtime_error("The vertex size is too large");
		}

		size_t q = 3;
		std::vector<std::pair<uint64_t, SpookyHash> > hash(q);
		for (std::pair<uint64_t, SpookyHash> & h : hash)
		{
			h.first = rand();
		}

		size_t edgeLength = vertexLength + 1;
		std::vector<bool> bitVector(filterSize, false);
		std::cerr << "Bloom filter counting..." << std::endl;
		for (const std::string & nowFileName: fileName)
		{
			for (StreamFastaParser parser(nowFileName); parser.ReadRecord();)
			{
				char ch;
				DnaString edge;
				for (size_t j = 0; j < edgeLength && parser.GetChar(ch); j++)
				{
					edge.AppendBack(ch);
				}

				if (edge.GetSize() == edgeLength)
				{
					while (true)
					{
						PutInBloomFilter(bitVector, hash, edge);
						if (parser.GetChar(ch))
						{
							edge.AppendBack(ch);
							edge.PopFront();
						}
						else
						{
							break;
						}
					}
				}
			}	
		}

		size_t mark = clock();
		std::cerr << "Passed: " << double(clock()) / CLOCKS_PER_SEC << std::endl;
		std::cerr << "Vertex enumeration...";
		for (const std::string & nowFileName : fileName)
		{
			bool start = true;
			for (StreamFastaParser parser(nowFileName); parser.ReadRecord(); start = true)
			{
				char ch;
				DnaString vertex;
				for (size_t j = 0; j < vertexLength && parser.GetChar(ch); j++)
				{
					vertex.AppendBack(ch);
				}

				if (vertex.GetSize() >= vertexLength)
				{
					while (true)
					{
						if (start)
						{
							bifurcation_.push_back(vertex.GetBody());
						}
						else
						{
							bool isBifurcation = false;							
							size_t inCount = 0;
							size_t outCount = 0;
							for (char ch : DnaString::LITERAL)
							{
								DnaString inEdge = vertex;
								DnaString outEdge = vertex;								
								inEdge.AppendFront(ch);
								outEdge.AppendBack(ch);
								inCount += IsInBloomFilter(bitVector, hash, inEdge) ? 1 : 0;
								outCount += IsInBloomFilter(bitVector, hash, outEdge) ? 1 : 0;
								if (inCount > 1 || outCount > 1)
								{
								    isBifurcation = true;
								    break;
								}
							}

							if (isBifurcation)
							{
								bifurcation_.push_back(vertex.GetBody());
							}
						}

						if (parser.GetChar(ch))
						{
							vertex.AppendBack(ch);
							vertex.PopFront();
							start = false;
						}
						else
						{
							bifurcation_.push_back(vertex.GetBody());
							break;
						}						
					}					
				}
			}
		}

		std::cout << "Passed: " << double(clock() - mark) / CLOCKS_PER_SEC  << std::endl;
		std::cout << "Sorting and duplicates removal..." << std::endl;
		mark = clock();
		std::sort(bifurcation_.begin(), bifurcation_.end());
		bifurcation_.erase(std::unique(bifurcation_.begin(), bifurcation_.end()), bifurcation_.end());
		std::cout << "Passed: " << double(clock() - mark) / CLOCKS_PER_SEC << std::endl;
	}

	size_t VertexEnumerator::GetVerticesCount() const
	{
		return bifurcation_.size();
	}

	size_t VertexEnumerator::GetId(const DnaString & vertex) const
	{
		std::vector<uint64_t>::const_iterator it = std::lower_bound(bifurcation_.begin(), bifurcation_.end(), vertex.GetBody());
		if (it == bifurcation_.end() || *it != vertex.GetBody())
		{
			return INVALID_VERTEX;
		}

		return it - bifurcation_.begin();
	}
}