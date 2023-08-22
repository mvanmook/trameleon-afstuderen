#ifndef TRAMELEON_MEDIAFOUNDATIONUTILS_H
#define TRAMELEON_MEDIAFOUNDATIONUTILS_H
#include <codecapi.h>
#include <cstdint>
#include <mfapi.h>

template <class T> int Release(T **ppT) {
  if (*ppT) {
    HRESULT hr = (*ppT)->Release();
    if (FAILED(hr)) { return -1; }
    *ppT = NULL;
  }
  return 0;
}

int convert_trameleon_to_mft_hardAcc(GUID &hardAcc, uint32_t type);
int convert_trameleon_to_mft_image_format(GUID &image_format, uint32_t type);
int convert_trameleon_to_mft_profile(uint32_t &profile, uint32_t type);
int convert_mft_to_trameleon_image_format(GUID &image_format, uint32_t &type);
#endif // TRAMELEON_MEDIAFOUNDATIONUTILS_H
