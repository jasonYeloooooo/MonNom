#include "CastStats.h"
#include <iostream>
#include <unordered_map>
#include "NomAlloc.h"
#include "NomNameRepository.h"
#include "llvm/Support/DynamicLibrary.h"
#include <vector>

#define RBUFSIZE 500
#ifdef _WIN32
#define TIMETYPE LONGLONG
#endif

using namespace std;

size_t castCount = 0;
size_t monoCastCount = 0;
size_t subtypingChecksCount = 0;
size_t typeArgumentRecursionsCount = 0;
size_t vtableAllocationsCount = 0;
size_t enoughSpaceCastsCount = 0;
size_t castingMonoCastsCount = 0;
size_t checkedMonoCastsCount = 0;
size_t intPacksCount = 0;
size_t intUnpacksCount = 0;
size_t intBoxesCount = 0;
size_t intUnboxesCount = 0;
size_t floatPacksCount = 0;
size_t floatUnpacksCount = 0;
size_t floatBoxesCount = 0;
size_t floatUnboxesCount = 0;
size_t timeUnitsInCasts = 0;

const std::string** debugFunNames;
size_t* profileCounterArray;
vector<size_t>* detailedProfileCounterArray;
TIMERTYPE lasttimestamp;
#ifdef _WIN32
TIMETYPE avgtime;
#endif
int64_t timestampcount = 0;

uint64_t general_allocations = 0;
uint64_t object_allocations = 0;
uint64_t closure_allocations = 0;
uint64_t record_allocations = 0;
uint64_t classtype_allocations = 0;

DLLEXPORT void RT_NOM_STATS_IncCasts()
{
	castCount++;
}

DLLEXPORT void RT_NOM_STATS_IncMonotonicCasts()
{
	monoCastCount++;
}

DLLEXPORT void RT_NOM_STATS_IncSubtypingChecks()
{
	subtypingChecksCount++;
}

DLLEXPORT void RT_NOM_STATS_IncTypeArgumentRecursions()
{
	typeArgumentRecursionsCount++;
}

DLLEXPORT void RT_NOM_STATS_IncVTableAlloactions()
{
	vtableAllocationsCount++;
}

DLLEXPORT void RT_NOM_STATS_IncEnoughSpaceCasts()
{
	enoughSpaceCastsCount++;
}

DLLEXPORT void RT_NOM_STATS_IncCastingMonoCasts()
{
	castingMonoCastsCount++;
}

DLLEXPORT void RT_NOM_STATS_IncCheckedMonoCasts()
{
	checkedMonoCastsCount++;
}

DLLEXPORT void RT_NOM_STATS_IncIntPacks()
{
	intPacksCount++;
}
DLLEXPORT void RT_NOM_STATS_IncIntUnpacks()
{
	intUnpacksCount++;
}
DLLEXPORT void RT_NOM_STATS_IncIntBoxes()
{
	intBoxesCount++;
}
DLLEXPORT void RT_NOM_STATS_IncIntUnboxes()
{
	intUnboxesCount++;
}
DLLEXPORT void RT_NOM_STATS_IncFloatPacks()
{
	floatPacksCount++;
}
DLLEXPORT void RT_NOM_STATS_IncFloatUnpacks()
{
	floatUnpacksCount++;
}
DLLEXPORT void RT_NOM_STATS_IncFloatBoxes()
{
	floatBoxesCount++;
}
DLLEXPORT void RT_NOM_STATS_IncFloatUnboxes()
{
	floatUnboxesCount++;
}
DLLEXPORT void RT_NOM_STATS_IncAllocations(AllocationType at)
{
	switch (at)
	{
	case AllocationType::General:
		general_allocations++;
		break;
	case AllocationType::Object:
		object_allocations++;
		break;
	case AllocationType::Lambda:
		closure_allocations++;
		break;
	case AllocationType::Struct:
		record_allocations++;
		break;
	case AllocationType::ClassType:
		classtype_allocations++;
		break;
	}
}
DLLEXPORT void RT_NOM_STATS_IncProfileCounter(size_t funnameid)
{
	profileCounterArray[funnameid]++;
}

#ifdef _WIN32
double find_timer_frequency()
{
	static double timer_frequency;
	static bool once = true;
	if (once)
	{
		once = false;
		LARGE_INTEGER ttf;
		if (!QueryPerformanceFrequency(&ttf))
		{
			throw new std::exception();
		}
		timer_frequency = (double)(ttf.QuadPart);
	}
	return timer_frequency;
}
#endif

DLLEXPORT TIMERTYPE RT_NOM_STATS_GetTimestamp()
{
#ifdef _WIN32
	LARGE_INTEGER timerVal;
	if (!QueryPerformanceCounter(&timerVal))
	{
		std::cout << "ERROR obtaining performance counter value";
		//throw new std::exception();
	}
	return timerVal;
#else
#ifdef CLOCK_THREAD_CPUTIME_ID
	struct timespec timerVal;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timerVal) != 0)
	{
		throw new std::exception();
	}
	return timerVal;
#else
	return clock();
#endif
#endif
}


DLLEXPORT void RT_NOM_STATS_IncCastTime(TIMERTYPE origTimerVal)
{
#ifdef _WIN32
	LARGE_INTEGER current;
	if (QueryPerformanceCounter(&current))
	{
		timeUnitsInCasts += current.QuadPart - origTimerVal.QuadPart;
		//double floatdiff = difference.QuadPart / Nom::Runtime::find_timer_frequency();
		//printf("%f Seconds\n", floatdiff);
		//return (void*)((intptr_t)(Nom::Runtime::NomJIT::Instance().getSymbolAddress("RT_NOM_VOIDOBJ")));
	}
	else
	{
		std::cout << "ERROR obtaining performance counter value";
		//throw new std::exception();
		//ThrowException(&stackframe, "Could not retrieve thread timings!");
	}
#else
#ifdef CLOCK_THREAD_CPUTIME_ID
	struct timespec current;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &current) == 0)
	{
		timeUnitsInCasts += ((current.tv_sec - origTimerVal.tv_sec) * 10000) + ((current.tv_nsec - origTimerVal.tv_nsec) / 100000);
		//double floatdiff = ((double)differenceMS) / 10000.0;
		//printf("%f Seconds\n", floatdiff);
		//return (void*)((intptr_t)(Nom::Runtime::NomJIT::Instance().getSymbolAddress("RT_NOM_VOIDOBJ")));
	}
	else
	{
		throw new std::exception();
	}
#else
	clock_t now = clock();
	timeUnitsInCasts += (long)(clock() - origTimerVal);
	//printf("%f Seconds, (%li ticks)\n", ((double)t) / CLOCKS_PER_SEC, t);
#endif
#endif
}

static size_t debugPrint_lastIndices[4][RBUFSIZE];
static size_t debugPrint_lastStrs[4][RBUFSIZE];
static int64_t debugPrint_lastValues[4][RBUFSIZE];
static NomDebugPrintValueType debugPrint_lastValueTypes[4][RBUFSIZE];
static decltype(NomDebugPrintLevel) debugPrint_lastLevel = 0;
static int debugPrint_rbufpos[4] = { 0, 0, 0, 0 };

struct ProgramPos
{
public:
	ProgramPos() {}
	ProgramPos(size_t funname, size_t linenum, double seconds) : FunName(funname), LineNum(linenum), Seconds(seconds)
	{

	}
	size_t FunName;
	size_t LineNum;
	double Seconds;
};

std::vector<ProgramPos> slowPositions;
double slowestSoFar = -1;

DLLEXPORT void RT_NOM_STATS_DebugLine(size_t funnameid, size_t linenum, NomDebugPrintValueType valueType, int64_t value, decltype(NomDebugPrintLevel) level)
{
	if (NomStatsLevel > 3)
	{
#ifdef _WIN32
		TIMERTYPE currentTime = RT_NOM_STATS_GetTimestamp();
		if (timestampcount > 0)
		{
			TIMETYPE difference = currentTime.QuadPart - lasttimestamp.QuadPart;
			if (difference > avgtime * 2 && timestampcount > 10)
			{
				double seconds = ((double)(difference - avgtime)) / find_timer_frequency();
				if (seconds > slowestSoFar || slowPositions.size() < 100)
				{
					auto iter = slowPositions.begin();
					auto posn = iter;
					bool found = false;
					for (; iter != slowPositions.end(); iter++)
					{
						auto pp = *iter;
						if (pp.FunName == funnameid && pp.LineNum == linenum)
						{
							if (pp.Seconds < seconds)
							{
								pp.Seconds = seconds;
								found = true;
							}
							found = true;
							break;
						}

						if (pp.Seconds < seconds && ! found)
						{
							auto npp = ProgramPos(funnameid, linenum, seconds);
							slowPositions.insert(iter, std::move(npp));
							cout << "SLOW: " << (*debugFunNames[funnameid]) << ":" << linenum << " - " << seconds << "s\n";
							found = true;
							break;
						}
					}
					if (!found&&slowPositions.size()<100)
					{
						auto npp = ProgramPos(funnameid, linenum, seconds);
						slowPositions.insert(iter, std::move(npp));
						cout << "SLOW: " << (*debugFunNames[funnameid]) << ":" << linenum << " - " << seconds << "s\n";
					}
					if (slowPositions.size() > 100)
					{
						slowPositions.resize(100);
					}
				}
			}
			if (timestampcount > 1)
			{
				avgtime += difference / timestampcount;
			}
			else
			{
				avgtime = difference;
			}
		}
		timestampcount++;
#endif
	}
	if (level <= 3)
	{
		debugPrint_lastLevel = level;
		debugPrint_lastIndices[level][debugPrint_rbufpos[level]] = linenum;
		debugPrint_lastStrs[level][debugPrint_rbufpos[level]] = funnameid;
		debugPrint_lastValues[level][debugPrint_rbufpos[level]] = value;
		debugPrint_lastValueTypes[level][debugPrint_rbufpos[level]] = valueType;
		debugPrint_rbufpos[level] = (debugPrint_rbufpos[level] + 1) % RBUFSIZE;
	}
	if (NomDebugPrintLevel >= level)
	{
		std::cout << *(debugFunNames[funnameid]) << ":" << std::dec << linenum << "\n";
		switch (valueType)
		{
		case NomDebugPrintValueType::Pointer:
			std::cout << "   Pointer: " << std::hex << value << "\n";
			break;
		case NomDebugPrintValueType::Int:
			std::cout << "   Int: " << std::dec << value << "\n";
			break;
		case NomDebugPrintValueType::Float:
			std::cout << "   Float: " << std::hex << value << "\n";
			break;
		case NomDebugPrintValueType::Bool:
			std::cout << "   Bool: " << std::dec << value << "\n";
			break;
		case NomDebugPrintValueType::Nothing:
			break;
		}
		std::cout.flush();
	}
	if (NomStatsLevel > 1)
	{
		RT_NOM_STATS_IncProfileCounter(funnameid);
		if (NomStatsLevel > 2)
		{
			vector<size_t>& sizevec = detailedProfileCounterArray[funnameid];
			if (sizevec.size() < linenum + 1)
			{
				sizevec.resize(linenum + 10, 0);
			}
			sizevec[linenum]++;
		}
	}
	if (NomStatsLevel > 3)
	{
		lasttimestamp = RT_NOM_STATS_GetTimestamp();
	}
	return;
}
DLLEXPORT int RT_NOM_STATS_Print(int level)
{
	int rbufpos = debugPrint_rbufpos[level];
	int target = rbufpos - 1;
	if (target < 0)
	{
		target = RBUFSIZE - 1;
	}
	while (rbufpos != target)
	{
		auto funnameid = debugPrint_lastStrs[level][rbufpos];
		if (funnameid == 0)
		{
			rbufpos = (rbufpos + 1) % RBUFSIZE;
			continue;
		}
		auto linenum = debugPrint_lastIndices[level][rbufpos];
		auto value = debugPrint_lastValues[level][rbufpos];
		auto valueType = debugPrint_lastValueTypes[level][rbufpos];
		std::cout << *(debugFunNames[funnameid]) << ":" << std::dec << linenum << "\n";
		switch (valueType)
		{
		case NomDebugPrintValueType::Pointer:
			std::cout << "   Pointer: " << std::hex << value << "\n";
			break;
		case NomDebugPrintValueType::Int:
			std::cout << "   Int: " << std::dec << value << "\n";
			break;
		case NomDebugPrintValueType::Float:
			std::cout << "   Float: " << std::hex << value << "\n";
			break;
		case NomDebugPrintValueType::Bool:
			std::cout << "   Bool: " << std::dec << value << "\n";
			break;
		case NomDebugPrintValueType::Nothing:
			break;
		}
		rbufpos = (rbufpos + 1) % RBUFSIZE;
	}
	{
		auto funnameid = debugPrint_lastStrs[level][rbufpos];
		if (funnameid != 0)
		{
			rbufpos = (rbufpos + 1) % RBUFSIZE;
			auto linenum = debugPrint_lastIndices[level][rbufpos];
			auto value = debugPrint_lastValues[level][rbufpos];
			auto valueType = debugPrint_lastValueTypes[level][rbufpos];
			std::cout << *(debugFunNames[funnameid]) << ":" << std::dec << linenum << "\n";
			switch (valueType)
			{
			case NomDebugPrintValueType::Pointer:
				std::cout << "   Pointer: " << std::hex << value << "\n";
				break;
			case NomDebugPrintValueType::Int:
				std::cout << "   Int: " << std::dec << value << "\n";
				break;
			case NomDebugPrintValueType::Float:
				std::cout << "   Float: " << std::hex << value << "\n";
				break;
			case NomDebugPrintValueType::Bool:
				std::cout << "   Bool: " << std::dec << value << "\n";
				break;
			case NomDebugPrintValueType::Nothing:
				break;
			}
		}
	}
	std::cout << "------------------------------------------------------\n";
	std::cout.flush();
	return rbufpos;
}

using namespace llvm;
using namespace std;
namespace Nom
{
	namespace Runtime
	{
		void InitializeProfileCounter()
		{
			llvm::sys::DynamicLibrary::AddSymbol("RT_NOM_STATS_DebugLine", (void*)&RT_NOM_STATS_DebugLine);
			auto maxid = NomNameRepository::ProfilingInstance().GetMaxID();
			debugFunNames = (const std::string**) nmalloc(sizeof(std::string*) * (maxid + 1));
			profileCounterArray = (size_t*)nmalloc(sizeof(size_t) * (maxid + 1));
			if (NomStatsLevel >= 3)
			{
				detailedProfileCounterArray = new vector<size_t>[maxid + 1];
			}
			for (decltype(maxid) i = 1; i <= maxid; i++)
			{
				debugFunNames[i] = NomNameRepository::ProfilingInstance().GetNameFromID(i);
				profileCounterArray[i] = 0;

			}
		}
		void PrintCastStats()
		{
			cout << "\n\nCasting Stats:\n----------------------";
			cout << "\n#Total Casts: " << std::dec << castCount;
			cout << "\n#Monotonic Casts: " << std::dec << monoCastCount;
			cout << "\n#Subtyping Checks: " << std::dec << subtypingChecksCount;
			cout << "\n#Type-Arg Recursions: " << std::dec << typeArgumentRecursionsCount;
			cout << "\n#VTable Allocations: " << std::dec << vtableAllocationsCount;
			cout << "\n#Mono casts using preallocation: " << std::dec << enoughSpaceCastsCount;
			cout << "\n#Casting Mono Casts: " << std::dec << castingMonoCastsCount;
			cout << "\n#Checked Mono Casts: " << std::dec << checkedMonoCastsCount;
			cout << "\n#Int Packs: " << std::dec << intPacksCount;
			cout << "\n#Int Unpacks: " << std::dec << intUnpacksCount;
			cout << "\n#Int Boxes: " << std::dec << intBoxesCount;
			cout << "\n#Int Unboxes: " << std::dec << intUnboxesCount;
			cout << "\n#Float Packs: " << std::dec << floatPacksCount;
			cout << "\n#Float Unpacks: " << std::dec << floatUnpacksCount;
			cout << "\n#Float Boxes: " << std::dec << floatBoxesCount;
			cout << "\n#Float Unboxes: " << std::dec << floatUnboxesCount;
			cout << "\n\nAllocation Stats:\n----------------------";
			cout << "\n#General Allocations: " << std::dec << general_allocations;
			cout << "\n#Object Allocations: " << std::dec << object_allocations;
			cout << "\n#Closure Allocations: " << std::dec << closure_allocations;
			cout << "\n#Record Allocations: " << std::dec << record_allocations;
			cout << "\n#Class Type Allocations: " << std::dec << classtype_allocations;
			cout << "\n#Total Allocations: " << std::dec << (general_allocations + object_allocations + closure_allocations + record_allocations + classtype_allocations);

#ifdef _WIN32
			double castSeconds = timeUnitsInCasts / find_timer_frequency();
#else
#ifdef CLOCK_THREAD_CPUTIME_ID
			double castSeconds = ((double)timeUnitsInCasts) / 10000.0;
#else
			double castSeconds = ((double)timeUnitsInCasts) / CLOCKS_PER_SEC;
#endif
#endif
			cout << "\nTime spent in casts: " << std::dec << castSeconds << " Seconds";

			cout << "\n";

			if (NomStatsLevel > 1)
			{
				cout << "\nProfile Summary:\n----------------------\n\n";
				auto maxid = NomNameRepository::ProfilingInstance().GetMaxID();
				for (decltype(maxid) i = 1; i <= maxid; i++)
				{
					cout << (*debugFunNames[i]) << ": " << std::dec << profileCounterArray[i] << "\n";
				}

				if (NomStatsLevel > 3)
				{
					cout << "\n\nSlow Timings:\n----------------------\n\n";
					for (auto pp : slowPositions)
					{
						cout << (*debugFunNames[pp.FunName]) << ":" << pp.LineNum << " - " << pp.Seconds << "s\n";
					}
				}

				if (NomStatsLevel > 2)
				{
					cout << "\n\nProfile Data:\n----------------------\n\n";
					for (decltype(maxid) i = 1; i <= maxid; i++)
					{
						cout << (*debugFunNames[i]) << ": " << std::dec << profileCounterArray[i] << "\n";
						vector<size_t>& dpca = detailedProfileCounterArray[i];
						if (dpca.size() > 0)
						{
							for (int j = 0; j < dpca.size(); j++)
							{
								if (dpca[j] > 0)
								{
									cout << "    " << j << ": " << dpca[j] << "\n";
								}
							}
						}
					}
				}
			}
		}
		llvm::Function* GetIncCastsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncCasts");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncCasts", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncMonotonicCastsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncMonotonicCasts");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncMonotonicCasts", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncSubtypingChecksFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncSubtypingChecks");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncSubtypingChecks", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncTypeArgumentRecursionsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncTypeArgumentRecursions");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncTypeArgumentRecursions", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncVTableAlloactionsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncVTableAlloactions");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncVTableAlloactions", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncEnoughSpaceCastsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncEnoughSpaceCasts");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncEnoughSpaceCasts", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncCastingMonoCastsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncCastingMonoCasts");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncCastingMonoCasts", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncCheckedMonoCastsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncCheckedMonoCasts");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncCheckedMonoCasts", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncIntPacksFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncIntPacks");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncIntPacks", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncIntUnpacksFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncIntUnpacks");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncIntUnpacks", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncIntBoxesFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncIntBoxes");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncIntBoxes", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncIntUnboxesFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncIntUnboxes");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncIntUnboxes", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncFloatPacksFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncFloatPacks");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncFloatPacks", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncFloatUnpacksFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncFloatUnpacks");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncFloatUnpacks", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncFloatBoxesFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncFloatBoxes");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncFloatBoxes", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncFloatUnboxesFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncFloatUnboxes");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncFloatUnboxes", &mod);
			}
			return fun;
		}
		llvm::Function* GetGetTimestampFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_GetTimestamp");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(inttype(bitsin(TIMERTYPE)), false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_GetTimestamp", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncCastTimeFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncCastTime");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), { inttype(bitsin(TIMERTYPE)) }, false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncCastTime", &mod);
			}
			return fun;
		}

		llvm::Function* GetDebugFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_DebugLine");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), { numtype(size_t), numtype(size_t), numtype(NomDebugPrintValueType), numtype(int64_t), numtype(decltype(NomDebugPrintLevel)) }, false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_DebugLine", &mod);
			}
			return fun;
		}
		llvm::Function* GetIncAllocationsFunction(llvm::Module& mod)
		{
			auto fun = mod.getFunction("RT_NOM_STATS_IncAllocations");
			if (fun == nullptr)
			{
				fun = Function::Create(FunctionType::get(Type::getVoidTy(LLVMCONTEXT), { inttype(bitsin(AllocationType)) }, false), GlobalValue::LinkageTypes::ExternalLinkage, "RT_NOM_STATS_IncAllocations", &mod);
			}
			return fun;
		}
	}
}