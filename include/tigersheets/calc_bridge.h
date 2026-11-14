#ifndef TIGERSHEETS_CALC_BRIDGE_H
#define TIGERSHEETS_CALC_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TigerSheetsCalcHandle TigerSheetsCalcHandle;
uint32_t tigersheets_calc_abi_version(void);
/* Registered function names and provenance; availability is not conformance.
 * Release the returned UTF-8 JSON using tigersheets_calc_free. */
char* tigersheets_calc_functions(void);
TigerSheetsCalcHandle* tigersheets_calc_new(void);
void tigersheets_calc_destroy(TigerSheetsCalcHandle* handle);
/* Returned UTF-8 JSON is owned by the bridge; release with tigersheets_calc_free.
 * The handle and NUL-terminated input must remain valid for the call. */
char* tigersheets_calc_calculate(const TigerSheetsCalcHandle* handle, const char* input);
/* Structural arithmetic/SUM qualification; query has variables/outputs arrays. */
char* tigersheets_calc_qualify_affine(const TigerSheetsCalcHandle* handle,const char* input,const char* query);
/* Pinned native number formatter; returned JSON contains status/text. */
char* tigersheets_calc_format_number(double value,const char* format,const char* locale);
/* Invoked synchronously for unknown scalar worksheet functions. The request is
 * JSON {name,args}. Return JSON {status:"completed",value} or {status:"error"}.
 * Response storage must live until the callback returns again or calculation
 * ends; the bridge consumes it immediately and never frees it. */
typedef const char* (*TigerSheetsFunctionCallback)(void* context, const char* request);
char* tigersheets_calc_calculate_with_functions(const TigerSheetsCalcHandle* handle,
    const char* input, TigerSheetsFunctionCallback callback, void* context);
/* Native reference-aware functions are independent of enabled VBA functions. */
char* tigersheets_calc_calculate_with_references(const TigerSheetsCalcHandle* handle,
    const char* input,TigerSheetsFunctionCallback callback,TigerSheetsFunctionCallback reference_callback,void* context);
/* Thread-safe cancellation is cooperative before/after evaluation and while
 * importing/exporting cells. Destroy/reset only after the active call ends. */
void tigersheets_calc_cancel(const TigerSheetsCalcHandle* handle);
void tigersheets_calc_reset_cancel(const TigerSheetsCalcHandle* handle);
void tigersheets_calc_free(char* text);

#ifdef __cplusplus
}
#endif
#endif
