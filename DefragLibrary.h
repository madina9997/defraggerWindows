#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <Windows.h>
#include <experimental\filesystem>
#include <signal.h>

using TString = std::basic_string<TCHAR>;

volatile sig_atomic_t Run1;

__declspec(dllexport) struct  TQuad
{
	//0: emty, 1:belong to nonfragmented file, 2:belongs to fragmented file, 3:metadata
	std::uint64_t State=0;
	std::uint64_t BeginCluster, EndCluster;
};

struct TRetrievalPointers;
struct TExtent {
	std::int64_t StartingLcn, Size, NextVcn;
};



class TRetrievalPointers {
public:
	TRetrievalPointers() = default;

	explicit TRetrievalPointers(PRETRIEVAL_POINTERS_BUFFER Buffer) :
		ExtentCount(Buffer->ExtentCount),
		StartingVcn(Buffer->StartingVcn.QuadPart),
		Extents(ExtentCount) {
		std::int64_t PreviousVcn = StartingVcn;
		for (auto i = 0LU; i < ExtentCount; i++) {
			Extents[i].Size = Buffer->Extents[i].NextVcn.QuadPart - PreviousVcn;
			Extents[i].StartingLcn = Buffer->Extents[i].Lcn.QuadPart;
			Extents[i].NextVcn = Buffer->Extents[i].NextVcn.QuadPart;
			PreviousVcn = Buffer->Extents[i].NextVcn.QuadPart;
		}

		std::sort(std::begin(Extents), std::end(Extents), [](TExtent A, TExtent B) {
			return A.NextVcn < B.NextVcn;
		});
	}

	long ExtentCount = 0;
	std::int64_t StartingVcn = 0;
	std::vector< TExtent > Extents;
};

struct TBitmap {
public:
	std::vector<bool> Buffer;
	std::vector<std::string> Buffer1;
	std::vector<int> Buffer2;
	TBitmap() = default;

	explicit TBitmap(PVOLUME_BITMAP_BUFFER Bitmap) : 
		StartingLcn(Bitmap->StartingLcn.QuadPart), 
		Buffer1(Bitmap->BitmapSize.QuadPart),
		Buffer2(Bitmap->BitmapSize.QuadPart),
		Buffer(Bitmap->BitmapSize.QuadPart) {
		for (std::int64_t i = 0; i < Buffer.size(); i++) {
			Buffer[i] = Bitmap->Buffer[i / 8] & (1 << (i % 8));
		} 
	}
	
	decltype(auto) TBitmap::operator[](std::int64_t i) {
		return Buffer[i];
	}
	
	std::int64_t Size() const {
		return Buffer.size();
	}

	std::int64_t StartingLcn = 0;
};

struct TVolume {
	TCHAR Name[MAX_PATH + 1];
	TString Path;
	TVolume(const TCHAR(&Name)[MAX_PATH + 1]);
	void SetPath(const TString &Paths);
	//std::vector<TQuad> TraverseFiles(const size_t NumberOfQuads) const;
	std::vector<TQuad> TraverseFilesToGetQuads(const size_t NumberOfQuads) const;
	void Defragment();
	int PartialDefragment1(int);
	int PartialDefragment2(int);
	void Stop() {
		Run1 = 0;
	}

private:
	TBitmap Analyze();
	void TraverseFiles() const;
	std::pair<int, int> FirstFreeExtent(int boundary);
	void DefragmentFreeSpace(int leftboundary, int);
	void FillFreeSpace(int, int);
	int ReplaceExtent(HANDLE filehandle, TExtent extent, int boundary);
	std::string FindFile(int, TBitmap&);
	int PlaceExtent(HANDLE Handle, TExtent extent, int boundary, int rightboundary);// , int);
	
//	volatile sig_atomic_t Run;
};

struct TVolumeSimple {
	TCHAR Name[MAX_PATH + 1];
};

struct TValueString {
	TCHAR Data[MAX_PATH + 1];
};

extern "C" __declspec(dllexport) TQuad *GetColouredBlocks(TValueString VolumeName, uint32_t size);

extern "C" __declspec(dllexport) uint64_t PartialDefr1(TValueString VolumeName, uint64_t rate);

extern "C" __declspec(dllexport) uint64_t PartialDefr2(TValueString VolumeName, uint64_t rate);

extern "C" __declspec(dllexport) int GetAllVolumesArray(int len, TVolumeSimple array[]);
//extern "C" __declspec(dllexport) int DefragmentVolume(TVolumeSimple volume);

__declspec(dllexport) TString GetPathNames(const TVolume &Volume);

__declspec(dllexport) HANDLE GetVolumeHandle(TVolume);

__declspec(dllexport) TBitmap GetVolumeBitmap(HANDLE);

__declspec(dllexport) TRetrievalPointers GetRetrievalPointersBuffer(HANDLE);

extern "C" __declspec(dllexport) int GetAllVolumesArrayGUID(int len, TVolumeSimple array[]);

HANDLE GetFileHandle(const std::experimental::filesystem::directory_entry &Entry);

HANDLE GetFileHandle1(const std::experimental::filesystem::path &Entry);

extern "C" __declspec(dllexport) void InterruptDefrag();

extern "C" __declspec(dllexport) uint64_t GetVolumeSizeInBytes(TValueString VolumeName);