#pragma once

#include <stdint.h>

enum class OTBMVersion
{
  Unknown = -1,
  OTBM1 = 0,
  OTBM2 = 1,
  OTBM3 = 2,
  OTBM4 = 3,
};

enum class ClientVersion
{
  CLIENT_VERSION_NONE = -1,
  CLIENT_VERSION_ALL = -2,
  CLIENT_VERSION_750 = 1,
  CLIENT_VERSION_755 = 2,
  CLIENT_VERSION_760 = 3,
  CLIENT_VERSION_770 = 3,
  CLIENT_VERSION_780 = 4,
  CLIENT_VERSION_790 = 5,
  CLIENT_VERSION_792 = 6,
  CLIENT_VERSION_800 = 7,
  CLIENT_VERSION_810 = 8,
  CLIENT_VERSION_811 = 9,
  CLIENT_VERSION_820 = 10,
  CLIENT_VERSION_830 = 11,
  CLIENT_VERSION_840 = 12,
  CLIENT_VERSION_841 = 13,
  CLIENT_VERSION_842 = 14,
  CLIENT_VERSION_850 = 15,
  CLIENT_VERSION_854_BAD = 16,
  CLIENT_VERSION_854 = 17,
  CLIENT_VERSION_855 = 18,
  CLIENT_VERSION_860_OLD = 19,
  CLIENT_VERSION_860 = 20,
  CLIENT_VERSION_861 = 21,
  CLIENT_VERSION_862 = 22,
  CLIENT_VERSION_870 = 23,
  CLIENT_VERSION_871 = 24,
  CLIENT_VERSION_872 = 25,
  CLIENT_VERSION_873 = 26,
  CLIENT_VERSION_900 = 27,
  CLIENT_VERSION_910 = 28,
  CLIENT_VERSION_920 = 29,
  CLIENT_VERSION_940 = 30,
  CLIENT_VERSION_944_V1 = 31,
  CLIENT_VERSION_944_V2 = 32,
  CLIENT_VERSION_944_V3 = 33,
  CLIENT_VERSION_944_V4 = 34,
  CLIENT_VERSION_946 = 35,
  CLIENT_VERSION_950 = 36,
  CLIENT_VERSION_952 = 37,
  CLIENT_VERSION_953 = 38,
  CLIENT_VERSION_954 = 39,
  CLIENT_VERSION_960 = 40,
  CLIENT_VERSION_961 = 41,
  CLIENT_VERSION_963 = 42,
  CLIENT_VERSION_970 = 43,
  CLIENT_VERSION_980 = 44,
  CLIENT_VERSION_981 = 45,
  CLIENT_VERSION_982 = 46,
  CLIENT_VERSION_983 = 47,
  CLIENT_VERSION_985 = 48,
  CLIENT_VERSION_986 = 49,
  CLIENT_VERSION_1010 = 50,
  CLIENT_VERSION_1020 = 51,
  CLIENT_VERSION_1021 = 52,
  CLIENT_VERSION_1030 = 53,
  CLIENT_VERSION_1031 = 54,
  CLIENT_VERSION_1035 = 55,
  CLIENT_VERSION_1076 = 56,
  CLIENT_VERSION_1098 = 57,
};

struct MapVersion
{
  MapVersion() : otbmVersion(OTBMVersion::OTBM4), clientVersion(ClientVersion::CLIENT_VERSION_NONE) {}
  MapVersion(OTBMVersion otbmVersion, ClientVersion clientVersion) : otbmVersion(otbmVersion), clientVersion(clientVersion) {}
  OTBMVersion otbmVersion;
  ClientVersion clientVersion;
};