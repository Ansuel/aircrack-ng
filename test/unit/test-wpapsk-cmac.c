#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "aircrack-util/common.h"
#include "aircrack-util/trampoline.h"
#include "aircrack-crypto/crypto_engine.h"
#include "aircrack-util/crypto_engine_loader.h"

void perform_unit_testing(void **state)
{
	wpapsk_password key[MAX_KEYS_PER_CRYPT_SUPPORTED];
	uint8_t mic[8][20];
	uint8_t expected_mic[20] =
		"\x2e\x13\xc4\x0c\xa1\xc2\xe4\xe2\x03\x7f\x99\xa2\xda\x18\xa4\x6b";
	uint8_t stmac[6] = "\x2c\xf0\xa2\xdd\xbc\xd0";
	uint8_t snonce[32] =
		"\x64\x67\x23\x3e\x73\x07\x67\xc3\x3e\x1d\xf8\x75\xc3\xad\x0e\xb5"
		"\x8a\x51\xad\x70\x4a\x3f\xae\x06\xb8\x18\xc0\xc5\xfc\xeb\xf3\xaf";
	uint8_t anonce[32] =
		"\x02\x18\xc7\xb6\x4e\xce\xf4\x0c\x4f\x15\x91\x5f\xbc\xeb\x19\xc8"
		"\xd6\x26\x08\x38\x7e\xb6\xb9\x86\xd9\x59\x9a\x8b\xd7\x0d\xc8\x5d";
	uint8_t eapol[256] =
		"\x02\x03\x00\x75\x02\x01\x0b\x00\x10\x00\x00\x00\x00\x00\x00\x00"
		"\x03\x64\x67\x23\x3e\x73\x07\x67\xc3\x3e\x1d\xf8\x75\xc3\xad\x0e"
		"\xb5\x8a\x51\xad\x70\x4a\x3f\xae\x06\xb8\x18\xc0\xc5\xfc\xeb\xf3"
		"\xaf\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x2e\x13\xc4\x0c\xa1\xc2\xe4\xe2\x03\x7f\x99\xa2\xda\x18\xa4"
		"\x6b\x00\x16\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac"
		"\x04\x01\x00\x00\x0f\xac\x06\x8c\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	uint32_t eapol_size = 117;
	ac_crypto_engine_t engine;
	uint8_t bssid[6] = "\xb0\xb9\x8a\x56\x8d\xea";
	uint8_t essid[33] = "Neheb";
	int nparallel = dso_ac_crypto_engine_simd_width();

	memset(&engine, 0, sizeof(engine));
	dso_ac_crypto_engine_init(&engine);
	dso_ac_crypto_engine_set_essid(&engine, &essid[0]);
	dso_ac_crypto_engine_thread_init(&engine, 1);
	dso_ac_crypto_engine_calc_pke(&engine, bssid, stmac, anonce, snonce, 1);

	for (int i = 0; i < nparallel; ++i)
	{
		int rc = -1;

		memset(key, 0, sizeof(key));

		strcpy((char*) (key[i].v), "bo$$password");
		key[i].length = 8;

		if ((rc = dso_ac_crypto_engine_wpa_crack(&engine,
		                                         key,
		                                         eapol,
		                                         eapol_size,
		                                         mic,
		                                         3,
		                                         expected_mic,
		                                         nparallel,
		                                         1))
		    >= 0)
		{
			// does the returned SIMD lane equal where we placed the key?
			assert_int_equal(rc, i);
		}
		else
		{
			fail();
		}
	}

	dso_ac_crypto_engine_thread_destroy(&engine, 1);
	dso_ac_crypto_engine_destroy(&engine);
}

void perform_unit_testing_for(void **state, int simd_flag)
{
	int simd_features = (int) ((uintptr_t) *state);

	// load the DSO
	ac_crypto_engine_loader_load(simd_flag);

	// Check if this shared library CAN run on the machine, if not; skip testing it.
	if (simd_features < dso_ac_crypto_engine_supported_features())
	{
		// unit-test cannot run without an illegal instruction.
		skip();
	}
	else
	{
		// Perform the unit-testing; we can run without an illegal instruction exception.
		perform_unit_testing(state);
	}

#if !defined(SANITIZE_ADDRESS)
	ac_crypto_engine_loader_unload();
#endif
}

void test_crypto_engine_x86_avx512f(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_AVX512F);
}

void test_crypto_engine_x86_avx2(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_AVX2);
}

void test_crypto_engine_x86_avx(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_AVX);
}

void test_crypto_engine_x86_sse2(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_SSE2);
}

void test_crypto_engine_arm_neon(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_NEON);
}

void test_crypto_engine_ppc_altivec(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_ALTIVEC);
}

void test_crypto_engine_ppc_power8(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_POWER8);
}

void test_crypto_engine_generic(void **state)
{
	perform_unit_testing_for(state, SIMD_SUPPORTS_NONE);
}

int group_setup(void **state)
{
	*state = (void*) ((uintptr_t) simd_get_supported_features());

	return 0;
}

int main(int argc, char *argv[])
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_crypto_engine_generic),
#if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86)
		cmocka_unit_test(test_crypto_engine_x86_avx2),
		cmocka_unit_test(test_crypto_engine_x86_avx),
		cmocka_unit_test(test_crypto_engine_x86_sse2),
#elif defined(__arm) || defined(__aarch64) || defined(__aarch64__)
		cmocka_unit_test(test_crypto_engine_arm_neon),
#elif defined(__PPC__) || defined(__PPC64__)
		cmocka_unit_test(test_crypto_engine_ppc_altivec),
		cmocka_unit_test(test_crypto_engine_ppc_power8),
#else
#warning "SIMD not available."
#endif
	};
	return cmocka_run_group_tests(tests, group_setup, NULL);
}
