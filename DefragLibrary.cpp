#include "DefragLibrary.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <experimental\filesystem>
#include <assert.h>

#include <tchar.h>

#ifdef min
#undef min
#endif

namespace fs = std::experimental::filesystem;

TString GetPathNames(const TVolume &Volume) {
	DWORD Size = 0;
	TString Paths(MAX_PATH + 1, '\0');

	if (GetVolumePathNamesForVolumeName(Volume.Name, Paths.data(), Paths.size(), &Size)) {
		return Paths;
	}
	if (GetLastError() != ERROR_MORE_DATA) {
		throw 1;
	}
	Paths.resize(Size);
	if (!GetVolumePathNamesForVolumeName(Volume.Name, Paths.data(), Paths.size(), &Size)) {
		throw 1;
	}
	return Paths;
}


extern "C" __declspec(dllexport)
int GetAllVolumesArray(int len, TVolumeSimple array[])
{
	TCHAR Name[MAX_PATH + 1];

	auto Handle = FindFirstVolume(Name, ARRAYSIZE(Name));
	if (Handle == INVALID_HANDLE_VALUE) {
		throw 1;
	}

	std::vector<TVolume> Volumes;
	do {
		auto &&Volume = Volumes.emplace_back(Name);
		Volume.SetPath(GetPathNames(Volume));
	} while (FindNextVolume(Handle, Name, ARRAYSIZE(Name)));

	DWORD Error = GetLastError();
	if (Error != ERROR_NO_MORE_FILES) {
		throw Error;
	}
	FindVolumeClose(Handle);
	int i;
	for (i = 0; i < Volumes.size() && i < len; i++) {
		lstrcpy(array[i].Name, Volumes[i].Path.c_str());
	}

	return Volumes.size();
}

HANDLE GetVolumeHandle(TVolume Volume) {
	auto Index = _tcslen(Volume.Name) - 1;
	Volume.Name[Index] = '\0';
	auto Handle = CreateFile(Volume.Name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	Volume.Name[Index] = '\\';
	if (!Handle || Handle == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	return Handle;
}


HANDLE GetFileHandle(const fs::directory_entry &Entry) {
	auto Handle = CreateFile(Entry.path().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (!Handle || Handle == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	return Handle;
}

HANDLE GetFileHandle(const fs::path Path) {
	auto Handle = CreateFile(Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (!Handle || Handle == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	return Handle;
}

HANDLE GetFileHandle1(const fs::path &Entry) {
	auto Handle = CreateFile(Entry.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (!Handle || Handle == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	return Handle;
}

TBitmap GetVolumeBitmap(HANDLE Handle) {
	STARTING_LCN_INPUT_BUFFER LCN{};
	PVOLUME_BITMAP_BUFFER Buffer = (PVOLUME_BITMAP_BUFFER)malloc(sizeof(VOLUME_BITMAP_BUFFER));
	if (!Buffer) {
		return {};
	}

	auto Success = DeviceIoControl(Handle, FSCTL_GET_VOLUME_BITMAP,
									&LCN,
									sizeof(LCN),
									Buffer, sizeof(VOLUME_BITMAP_BUFFER), nullptr, nullptr);
	if (Success) {
		TBitmap Bitmap(Buffer);
		free(Buffer);
		return Bitmap;
	}
	if (GetLastError() == ERROR_MORE_DATA) {
		auto NumClusters = Buffer->BitmapSize.QuadPart - Buffer->StartingLcn.QuadPart;
		size_t Size = (size_t)NumClusters + sizeof(VOLUME_BITMAP_BUFFER);
		auto NBuffer = (PVOLUME_BITMAP_BUFFER)realloc(Buffer, Size);
		if (!NBuffer) {
			free(Buffer);
			return {};
		}
		if (DeviceIoControl(Handle, FSCTL_GET_VOLUME_BITMAP, &LCN, sizeof(LCN), Buffer = NBuffer, Size, nullptr, nullptr)) {
			TBitmap Bitmap(Buffer);
			free(Buffer);
			return Bitmap;
		}
	}
	free(Buffer);
	return {};
}

TRetrievalPointers GetRetrievalPointersBuffer(HANDLE Handle) {
	size_t Size = 64;
	PRETRIEVAL_POINTERS_BUFFER Buffer = (PRETRIEVAL_POINTERS_BUFFER)malloc(Size);
	if (!Buffer) {
		return {};
	}
	STARTING_VCN_INPUT_BUFFER VCN{};
	auto Success = DeviceIoControl(Handle, FSCTL_GET_RETRIEVAL_POINTERS, &VCN, sizeof(VCN), Buffer, Size, nullptr, nullptr);
	auto Error = GetLastError();
	while (!Success && Error == ERROR_MORE_DATA) {
		auto NBuffer = (PRETRIEVAL_POINTERS_BUFFER)realloc(Buffer, Size *= 2);
		if (!NBuffer) {
			free(Buffer);
			return {};
		}
		Success = DeviceIoControl(Handle, FSCTL_GET_RETRIEVAL_POINTERS, &VCN, sizeof(VCN), Buffer = NBuffer, Size, nullptr, nullptr);
		Error = GetLastError();
	}
	if (Error != ERROR_SUCCESS && Error != ERROR_MORE_DATA) {
		free(Buffer);
		return {};
	}
	TRetrievalPointers Pointers(Buffer);
	free(Buffer);
	return Pointers;
}

TVolume::TVolume(const TCHAR(&Name)[MAX_PATH + 1]) {
	memcpy(this->Name, Name, sizeof(Name));
	//this->ExtentsTable = new std::map<const std::pair<int64_t, int64_t>, std::string >();
}

void TVolume::SetPath(const TString &Paths) {
	auto End = std::find(std::cbegin(Paths), std::cend(Paths), '\0');
	Path.resize(std::distance(std::cbegin(Paths), End));
	std::copy(std::cbegin(Paths), End, std::begin(Path));
}
std::ostream &operator<<(std::ostream &Stream, TVolume &Volume) {
	return Stream << Volume.Name;
}

std::ostream &operator<<(std::ostream &Stream, const TRetrievalPointers &Pointers) {
	Stream << "{ExtentCount=" << Pointers.ExtentCount << /*", StartingVcn=" << Pointers.StartingVcn << */", Extents=[";
	for (auto &&E : Pointers.Extents) {
		Stream << "{Size=" << E.Size << ", STARTINGLCN=" << E.StartingLcn << /*" FirstPreviousFreeLcn=  " << E.FirstPreviousFreeLcn << " LastFreeLcn=  " << E.LastFreeLcn << */"}, ";
	}
	return Stream << "]}";
}

typedef std::pair<const std::pair<int64_t, int64_t>, std::string > globalExtents;

std::ostream &operator<<(std::ostream &os, const globalExtents  &t) {
	return os << "[" << t.first.first << " - " << t.first.second << "]  " << t.second;
}

std::ostream &operator<<(std::ostream &Stream, const TQuad &Quad) {
	Stream << Quad.BeginCluster << ' ' << Quad.EndCluster << "; [";

	Stream << Quad.State;

	return Stream << "]";
}
std::map<const std::pair<int64_t, int64_t>, std::string> * TraverseFilesDebug(TVolume volume){
	std::map<const std::pair<int64_t, int64_t>, std::string> ExtentsTable= std::map<const std::pair<int64_t,int64_t>, std::string>();
	auto Bitmap = GetVolumeBitmap(GetVolumeHandle(volume.Name));
	const auto NumberOfClusters = Bitmap.Size();

	fs::path Path = volume.Path;
	for (auto &&F : fs::recursive_directory_iterator(Path)) {
		if (!fs::is_directory(F)) {
			auto Handle = GetFileHandle(F);
			auto Buffer = GetRetrievalPointersBuffer(Handle);
			auto FileSize = GetFileSize(Handle, NULL);
			for (auto &&extent : Buffer.Extents) {
				//ExtentsTable.emplace(std::make_pair(extent.StartingLcn, extent.StartingLcn + extent.Size - 1), F.path().string());
				auto Start = extent.StartingLcn, End = extent.StartingLcn + extent.Size - 1;
				ExtentsTable.emplace(std::make_pair(Start,End), F.path().string());
			}
		}
	}
	return &ExtentsTable;
}

TBitmap TVolume::Analyze() {
	auto Bitmap = GetVolumeBitmap(GetVolumeHandle(this->Name));
	fs::path Path = this->Path;
	for (auto &&F : fs::recursive_directory_iterator(Path)) {
		if (!fs::is_directory(F)) {
			auto Handle = GetFileHandle(F);
			auto Buffer = GetRetrievalPointersBuffer(Handle);
			for (int i = 0; i < Buffer.ExtentCount; i++) {
				for (int j = Buffer.Extents[i].StartingLcn;
					j < Buffer.Extents[i].StartingLcn + Buffer.Extents[i].Size; j++) {
					Bitmap.Buffer1[j] = F.path().string();
					Bitmap.Buffer2[j] = 1;
				}
			}

		}
	}
	return Bitmap;
}

std::vector<TQuad> TVolume::TraverseFilesToGetQuads(const size_t NumberOfQuads) const {
	auto Bitmap = GetVolumeBitmap(	GetVolumeHandle(this->Name));
	const auto NumberOfClusters = Bitmap.Size();
	const auto SizeInClusters = NumberOfClusters / NumberOfQuads + !!(NumberOfClusters % NumberOfQuads);
	std::vector<TQuad> Quads1(NumberOfQuads);
	for (int i = 0; i < NumberOfQuads; i++) {
		Quads1[i].BeginCluster = i * SizeInClusters;
		Quads1[i].EndCluster = std::min((i + 1) * SizeInClusters - 1, NumberOfClusters);
		//Quads1[i].State = 0;
	}
	fs::path Path = this->Path;
	for (auto &&F : fs::recursive_directory_iterator(Path)) {
		if (!fs::is_directory(F)) {
			auto Handle = GetFileHandle(F);
			auto Buffer = GetRetrievalPointersBuffer(Handle);
			auto FileSize = GetFileSize(Handle, NULL);
			for (auto &&extent : Buffer.Extents) {
				auto Start = extent.StartingLcn, End = extent.StartingLcn + extent.Size - 1;
				for (int i = Start / SizeInClusters; i <= End / SizeInClusters; i++) {
					if (Buffer.ExtentCount == 1) Quads1[i].State = 1;
					else if (Buffer.ExtentCount > 1) {
						Quads1[i].State = 2;
					}
				}
			}
		}
	}
	/*for (auto quad : Quads1) {
		if (quad.State == 0) {
			for (int i = quad.BeginCluster; i < Bitmap.Size()&& i <quad.EndCluster+1; i++) {
				//assert(0 < i && i < Bitmap.Size());
				if (Bitmap[i]) {
					quad.State = 3;
					break;
				}
			}
		}

	}*/
	for (int i = 0; i < NumberOfQuads; i++) {
		if (Quads1[i].State ==0) {
			int lb = i * SizeInClusters;
			int rb= std::min((i + 1) * SizeInClusters - 1, NumberOfClusters);
			for (int j = lb; j < rb; j++) {
				if (Bitmap[j]) {
					Quads1[i].State = 3;
					break;
				}

			}
		}
	}

	return Quads1;
}

TQuad *GetColouredBlocks(TValueString VolumeName, uint32_t Size)
{
	TVolume Volume(VolumeName.Data);
	Volume.SetPath(GetPathNames(Volume));
	auto Quads = Volume.TraverseFilesToGetQuads(Size);

	auto Quads1 = new TQuad[Size];
	memcpy(Quads1, Quads.data(), Size * sizeof(TQuad));
	return Quads1;
}

extern "C" __declspec(dllexport)
int GetAllVolumesArrayGUID(int len, TVolumeSimple array[])
{
	TCHAR Name[MAX_PATH + 1];

	auto Handle = FindFirstVolume(Name, ARRAYSIZE(Name));
	if (Handle == INVALID_HANDLE_VALUE) {
		throw 1;
	}

	std::vector<TVolume> Volumes;
	do {
		auto &&Volume = Volumes.emplace_back(Name);
		Volume.SetPath(GetPathNames(Volume));
	} while (FindNextVolume(Handle, Name, ARRAYSIZE(Name)));

	DWORD Error = GetLastError();
	if (Error != ERROR_NO_MORE_FILES) {
		throw Error;
	}
	FindVolumeClose(Handle);
	int i;
	for (i = 0; i < Volumes.size() && i < len; i++) {
		lstrcpy(array[i].Name, Volumes[i].Name);
	}

	return Volumes.size();
}


std::pair<int, int> TVolume::FirstFreeExtent(int boundary) {
	auto Bitmap = GetVolumeBitmap(GetVolumeHandle(this->Name));
	int begin = -1, size = 0;
	for (int i = boundary; i < Bitmap.Size(); i++) {
		if (Bitmap[i] == 0 ) {
			begin = i; break;
		}
	}
	for (int i = begin; i != -1 && i < Bitmap.Size(); i++, size++) {
		if (Bitmap[i] != 0) {
			break;
		}
	}
	return std::make_pair(begin, size);
}

int TVolume::ReplaceExtent(HANDLE Handle, TExtent extent, int boundary) {
	int retBoundary = boundary;
	int placed = 0;
	for (std::pair<int, int> freeSpace = this->FirstFreeExtent(boundary);
		extent.Size>placed && freeSpace.second != 0;
		freeSpace = this->FirstFreeExtent(retBoundary)) {

		MOVE_FILE_DATA structure;
		structure.FileHandle = Handle;
		structure.StartingVcn.QuadPart = extent.NextVcn + placed - extent.Size;
		structure.StartingLcn.QuadPart = freeSpace.first;
		structure.ClusterCount = (freeSpace.second<extent.Size - placed) ? freeSpace.second : extent.Size - placed;
		retBoundary = structure.StartingLcn.QuadPart + structure.ClusterCount;
		placed += structure.ClusterCount;
		std::cout << "		clustercount = " << structure.ClusterCount << std::endl;
		auto result = DeviceIoControl(GetVolumeHandle(this->Name), FSCTL_MOVE_FILE,
			&structure, sizeof(structure), nullptr, 0, nullptr, nullptr);
	}
	return retBoundary;
}
void TVolume::DefragmentFreeSpace(int leftboundary, int rightboundary) {
	auto Bitmap = this->Analyze();
	int boundaryForSearchingFreeSpace = rightboundary;
	for (int clNum = leftboundary; clNum < rightboundary; ) {
		if ((Bitmap[clNum] == 0 || Bitmap[clNum] == 1) && Bitmap.Buffer2[clNum] == 0) { clNum++; continue; }
		fs::path Path(Bitmap.Buffer1[clNum]);
		auto Handle = GetFileHandle1(Path);
		auto Buffer = GetRetrievalPointersBuffer(Handle);
		for (auto &&extent : Buffer.Extents) {
			if (extent.StartingLcn >= clNum && extent.StartingLcn< rightboundary) {
				boundaryForSearchingFreeSpace =
					this->ReplaceExtent(Handle, extent, boundaryForSearchingFreeSpace);
				for (int extCls = extent.StartingLcn; extCls < extent.StartingLcn + extent.Size; extCls++) {
					Bitmap.Buffer2[extCls] = 0;
				}
			}
			else if (extent.StartingLcn < clNum && (clNum - extent.StartingLcn) <= extent.Size) {
				clNum++;
			}
		}
		CloseHandle(Handle);
	}
}

std::string TVolume::FindFile(int boundary, TBitmap& Bitmap) {
	for (int i = boundary; i < Bitmap.Size(); i++) {
		if (Bitmap[i] == 1 && Bitmap.Buffer2[i] == 1) {
			return Bitmap.Buffer1[i];
		}
	}
	return "";
}

int freeClustersInInterval(TBitmap& Bitmap, int leftboundary, int rightboundary) {
	int freeClustersAmount = 0;
	for (int i = leftboundary; i<rightboundary; i++) {
		if (Bitmap[i] == 0) freeClustersAmount++;
	}
	return freeClustersAmount;
}


int TVolume::PlaceExtent(HANDLE Handle, TExtent extent, int boundary, int rightBoundary) {
	int leftboundary = boundary;
	int rightboundary = rightBoundary;
	int placed = 0;
	int retBoundary = leftboundary;
	std::pair<int, int> freeSpace;
	for (freeSpace = this->FirstFreeExtent(leftboundary), retBoundary = freeSpace.first;
		extent.Size>placed && retBoundary<rightboundary;
		freeSpace = this->FirstFreeExtent(retBoundary)) {

		MOVE_FILE_DATA structure;
		structure.FileHandle = Handle;
		structure.StartingVcn.QuadPart = extent.NextVcn - extent.Size + placed;
		structure.StartingLcn.QuadPart = freeSpace.first;
		structure.ClusterCount = (freeSpace.second< extent.Size - placed) ? freeSpace.second : extent.Size - placed;
		retBoundary = structure.StartingLcn.QuadPart + structure.ClusterCount;
		placed += structure.ClusterCount;
		auto result = DeviceIoControl(GetVolumeHandle(this->Name), FSCTL_MOVE_FILE,
			&structure, sizeof(structure), nullptr, 0, nullptr, nullptr);
	}
	return retBoundary;
}

void TVolume::FillFreeSpace(int boundary, int rightboundary) {
	int leftboundary = boundary;
	auto Bitmap = this->Analyze();
	std::string file = this->FindFile(rightboundary, Bitmap);
	for (int i = leftboundary - 1; i > 0; i--) {
		if (Bitmap.Buffer2[i] == 1) {
			file = Bitmap.Buffer1[i];
			break;
		}
	}
	std::pair<int, int> actualLeftBoundary = this->FirstFreeExtent(leftboundary);
	if (actualLeftBoundary.first < rightboundary) {
		for (;
			leftboundary < rightboundary && file != "";
			file = this->FindFile(rightboundary, Bitmap)) {
			if (file.empty()) continue;
			fs::path Path(file);
			auto Handle = GetFileHandle1(Path);
			auto Buffer = GetRetrievalPointersBuffer(Handle);
			for (auto &&extent : Buffer.Extents) {
				if (extent.StartingLcn >= rightboundary) {
					if (leftboundary >= rightboundary) break;
					leftboundary = this->PlaceExtent(Handle, extent, leftboundary, rightboundary);//, freeClustersInInterval(Bitmap, leftboundary, rightboundary));
					for (int extCls = extent.StartingLcn;
						extCls < extent.StartingLcn + extent.Size; extCls++) {
						Bitmap.Buffer2[extCls] = 0;
					}
				}
			}
			CloseHandle(Handle);
		}
	}
}

void TVolume::Defragment() {
	int leftboundary = 0;
	TBitmap Bitmap = GetVolumeBitmap(GetVolumeHandle(this->Name));
	fs::space_info pathtoVolume = fs::space(this->Path);
	auto capacityinBytes = pathtoVolume.capacity;
	auto clusterSize = capacityinBytes / Bitmap.Size();
	auto availableSpace = pathtoVolume.available / clusterSize;
	availableSpace /= 2;
	auto busyClusters = (pathtoVolume.capacity - pathtoVolume.available) / clusterSize;
	Run1 = 1;


	for (int rightboundary = availableSpace;
		leftboundary<busyClusters && Run1;
		leftboundary = rightboundary,
			rightboundary += ((busyClusters - leftboundary) >= availableSpace) ? availableSpace : (busyClusters - leftboundary)) 
	{
		DefragmentFreeSpace(leftboundary, rightboundary);
		FillFreeSpace(leftboundary, rightboundary);
	}
}

int TVolume::PartialDefragment1(int rate) {
	TBitmap Bitmap = GetVolumeBitmap(GetVolumeHandle(this->Name));
	fs::space_info pathtoVolume = fs::space(this->Path);
	auto capacityinBytes = pathtoVolume.capacity;
	auto clusterSize = capacityinBytes / Bitmap.Size();
	auto availableSpace = pathtoVolume.available / clusterSize;
	availableSpace /= 2;
	int leftboundary = rate*availableSpace;
	auto busyClusters = (pathtoVolume.capacity - pathtoVolume.available) / clusterSize;
	if (leftboundary >= busyClusters) return 0;

	int rightboundary = leftboundary+ ((busyClusters - leftboundary) >= availableSpace) ? availableSpace : (busyClusters - leftboundary);
	{
		DefragmentFreeSpace(leftboundary, rightboundary);
	}
	return rate+1;
}

int TVolume::PartialDefragment2(int rate) {
	TBitmap Bitmap = GetVolumeBitmap(GetVolumeHandle(this->Name));
	fs::space_info pathtoVolume = fs::space(this->Path);
	auto capacityinBytes = pathtoVolume.capacity;
	auto clusterSize = capacityinBytes / Bitmap.Size();
	auto availableSpace = pathtoVolume.available / clusterSize;
	availableSpace /= 2;
	int leftboundary = rate*availableSpace;
	auto busyClusters = (pathtoVolume.capacity - pathtoVolume.available) / clusterSize;
	if (leftboundary >= busyClusters) return 0;
	int rightboundary = leftboundary + ((busyClusters - leftboundary) >= availableSpace) ? availableSpace : (busyClusters - leftboundary);
	{
		FillFreeSpace(leftboundary, rightboundary);
	}
	return rate+1;
}

uint64_t PartialDefr1(TValueString VolumeName, uint64_t rate)
{
	TVolume Volume(VolumeName.Data);
	Volume.SetPath(GetPathNames(Volume));
//	return Volume.PartialDefragment1(rate);
	Volume.Defragment();
	return 1;
}

void InterruptDefrag()
{
	Run1 = 0;

}

uint64_t PartialDefr2(TValueString VolumeName, uint64_t rate)
{
	TVolume Volume(VolumeName.Data);
	Volume.SetPath(GetPathNames(Volume));
	return Volume.PartialDefragment2(rate);
}

uint64_t GetVolumeSizeInBytes(TValueString VolumeName) {
	TVolume Volume(VolumeName.Data);
	Volume.SetPath(GetPathNames(Volume));
	fs::space_info pathtoVolume = fs::space(Volume.Path);
	return pathtoVolume.capacity;
}

uint64_t GetClusterSizeInBytes(TValueString VolumeName) {
	TVolume Volume(VolumeName.Data);
	Volume.SetPath(GetPathNames(Volume));
	fs::space_info pathtoVolume = fs::space(Volume.Path);
	return pathtoVolume.capacity;
}