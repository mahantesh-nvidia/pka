/*
 * Copyright 2008-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <openssl/opensslconf.h>

#include <stdio.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/engine.h>
#include <openssl/rsa.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/modes.h>
#include <openssl/ossl_typ.h>

#include "ec_local.h"
#include "pka_helper.h"

/* Attempt to have a single source for both 1.0 and 1.1 */
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
#  define BLUEFIELD_DYNAMIC_ENGINE
#else
# error "Only OpenSSL >= 1.0 is supported"
#endif

#ifdef BLUEFIELD_DYNAMIC_ENGINE
/* Engine id and name */
static const char *engine_pka_id = "pka";
static const char *engine_pka_name = "BlueField PKA engine support";

/* Engine lifetime functions */
static int engine_pka_destroy(ENGINE *e);
static int engine_pka_init(ENGINE *e);
static int engine_pka_finish(ENGINE *e);
void engine_load_pka_int(void);

/* BN operations */ 
/* BN mod_exp */
static int engine_pka_bn_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                            const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int
engine_pka_ecc_pt_add(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                      const EC_POINT *b, BN_CTX *ctx);
static int 
engine_pka_ecc_pt_mult(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
                       size_t num, const EC_POINT *points[],
                       const BIGNUM *scalars[], BN_CTX *ctx);


# if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
/* RSA stuff */
#ifndef NO_RSA
static int engine_pka_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                                    BN_CTX *ctx);

static int engine_pka_rsa_init(RSA *rsa);
static int engine_pka_rsa_finish(RSA *rsa);

static RSA_METHOD *pka_rsa_meth = NULL;
#endif
/* DH stuff */
#ifndef NO_DH
static int engine_pka_dh_bn_mod_exp(const DH *dh, BIGNUM *r, const BIGNUM *a,
                                    const BIGNUM *p, const BIGNUM *m,
                                    BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int engine_pka_dh_init(DH *dh);
static int engine_pka_dh_finish(DH *dh);
static DH_METHOD *pka_dh_meth = NULL;
#endif
/* DSA stuff */
#ifndef NO_DSA
static int engine_pka_dsa_mod_exp(DSA *dsa, BIGNUM *rr, const BIGNUM *a1,
                                  const BIGNUM *p1, const BIGNUM *a2,
                                  const BIGNUM *p2, const BIGNUM *m,
                                  BN_CTX *ctx, BN_MONT_CTX *in_mont);
static int engine_pka_dsa_bn_mod_exp(DSA *dsa, BIGNUM *r, const BIGNUM *a,
                                     const BIGNUM *p, const BIGNUM *m,
                                     BN_CTX *ctx, BN_MONT_CTX *m_ctx);
static int engine_pka_dsa_init(DSA *dsa);
static int engine_pka_dsa_finish(DSA *dsa);
static DSA_METHOD *pka_dsa_meth = NULL;
#endif
/* EC stuff */
#ifndef NO_EC
static int engine_pka_ecdh_compute_key(unsigned char **pout, size_t *poutlen,
                                       const EC_POINT *pub_key, const EC_KEY *ecdh);
static int engine_pka_ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                                       BIGNUM **rp);
static int engine_pka_ecdsa_sign(int type, const unsigned char *dgst, int dlen,
                                 unsigned char *sig, unsigned int *siglen,
                                 const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey);
static ECDSA_SIG *engine_pka_ecdsa_sign_sig(const unsigned char *dgst, int dgst_len,
                                            const BIGNUM *in_kinv, const BIGNUM *in_r,
                                            EC_KEY *eckey);
static int engine_pka_ecdsa_verify(int type, const unsigned char *dgst, int dgst_len,
                             const unsigned char *sigbuf, int sig_len, EC_KEY *eckey);
static int engine_pka_ecdsa_verify_sig(const unsigned char *dgst, int dgst_len,
                                       const ECDSA_SIG *sig, EC_KEY *eckey);
//static int engine_pka_ec_init(EC_KEY *key);
//static void engine_pka_ec_finish(EC_KEY *key);
static EC_KEY_METHOD        *pka_ec_key_meth = NULL;
static const EC_KEY_METHOD  *ec_key_meth     = NULL;
#endif
/* rand stuff */
# else /* OpenSSL >=1.0.0 && < 1.0.1 */
/* RSA stuff */
#ifndef NO_RSA
static RSA_METHOD pka_rsa_meth = {
    "BlueField RSA method",
    NULL,
    NULL,
    NULL,
    NULL,
    engine_pka_rsa_mod_exp,
    engine_pka_bn_mod_exp,
    engine_pka_rsa_init,
    engine_pka_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif
/* DH stuff */
#ifndef NO_DH
static DH_METHOD pka_dh_meth = {
    "BlueField DH method",
    NULL,
    NULL,
    engine_pka_dh_bn_mod_exp,
    engine_pka_dh_init,
    engine_pka_dh_finish,
    0,
    NULL,
    NULL
};
#endif
/* DSA stuff */
#ifndef NO_DSA
static DSA_METHOD pka_dsa_meth = {
    "BlueField DSA method",
    NULL,
    NULL,
    NULL,
    engine_pka_dsa_mod_exp,
    engine_pka_dsa_bn_mod_exp,
    engine_pka_dsa_init,
    engine_pka_dsa_finish,
    0,
    NULL,
    NULL,
    NULL
};
#endif
/* EC stuff */
#ifndef NO_EC
#endif
/* rand stuff */
# endif

static int bind_pka(ENGINE *e)
{
    int rc = 1;

# if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#ifndef NO_RSA
    /* Setup our RSA_METHOD that we provide pointers to */
    if ((pka_rsa_meth = RSA_meth_new("BlueField RSA method", 0)) == NULL
        || rc != RSA_meth_set_mod_exp(pka_rsa_meth, engine_pka_rsa_mod_exp)
        || rc != RSA_meth_set_bn_mod_exp(pka_rsa_meth, engine_pka_bn_mod_exp)
        || rc != RSA_meth_set_init(pka_rsa_meth, engine_pka_rsa_init)
        || rc != RSA_meth_set_finish(pka_rsa_meth, engine_pka_rsa_finish))
    {
        printf("ERROR: failed to setup BlueField RSA method\n");
        return 0;
    }

    /*
     * We know that the "RSA_PKCS1_OpenSSL()" functions hook properly
     * to the bluefield-specific engine_pka_rsa_mod_exp so we use
     * those functions.
     */
    const RSA_METHOD *meth  = RSA_PKCS1_OpenSSL();
    rc   &= RSA_meth_set_pub_enc(pka_rsa_meth, RSA_meth_get_pub_enc(meth));
    rc   &= RSA_meth_set_pub_dec(pka_rsa_meth, RSA_meth_get_pub_dec(meth));
    rc   &= RSA_meth_set_priv_enc(pka_rsa_meth, RSA_meth_get_priv_enc(meth));
    rc   &= RSA_meth_set_priv_dec(pka_rsa_meth, RSA_meth_get_priv_dec(meth));
    if (!rc)
    {
        printf("ERROR: failed to hook PKCS1_SSLeay() functions\n");
        return 0;
    }
#endif
#ifndef NO_DH
    /* Setup our DH_METHOD that we provide pointers to */
    if ((pka_dh_meth = DH_meth_new("BlueField DH method", 0)) == NULL
        || rc != DH_meth_set_bn_mod_exp(pka_dh_meth, engine_pka_dh_bn_mod_exp)
        || rc != DH_meth_set_init(pka_dh_meth, engine_pka_dh_init)
        || rc != DH_meth_set_finish(pka_dh_meth, engine_pka_dh_finish))
    {
        printf("ERROR: failed to setup BlueField DH method\n");
        return 0;
    }

    /*
     * We know that the "DH_OpenSSL()" functions hook properly
     * to the bluefield-specific engine_pka_rsa_mod_exp so we use
     * those functions.
     */
    const DH_METHOD *dh_meth  = DH_OpenSSL();
    rc   &= DH_meth_set_generate_key(pka_dh_meth, DH_meth_get_generate_key(dh_meth));
    rc   &= DH_meth_set_compute_key(pka_dh_meth, DH_meth_get_compute_key(dh_meth));
    if (!rc)
    {
        printf("ERROR: failed to hook DH_OpenSSL() functions\n");
        return 0;
    }
#endif
#ifndef NO_DSA
    /* Setup our DSA_METHOD that we provide pointers to */
    if ((pka_dsa_meth = DSA_meth_new("BlueField DSA method", 0)) == NULL
        || rc != DSA_meth_set_mod_exp(pka_dsa_meth, engine_pka_dsa_mod_exp)
        || rc != DSA_meth_set_bn_mod_exp(pka_dsa_meth, engine_pka_dsa_bn_mod_exp)
        || rc != DSA_meth_set_init(pka_dsa_meth, engine_pka_dsa_init)
        || rc != DSA_meth_set_finish(pka_dsa_meth, engine_pka_dsa_finish))
    {
        printf("ERROR: failed to setup BlueField DH method\n");
        return 0;
    }

    const DSA_METHOD *dsa_meth = DSA_OpenSSL();
    rc &= DSA_meth_set_sign(pka_dsa_meth, DSA_meth_get_sign(dsa_meth));
    rc &= DSA_meth_set_sign_setup(pka_dsa_meth, DSA_meth_get_sign_setup(dsa_meth));
    rc &= DSA_meth_set_verify(pka_dsa_meth, DSA_meth_get_verify(dsa_meth));
    if (!rc)
    {
        printf("ERROR: failed to hook DSA_OpenSSL() functions\n");
        return 0;
    }
#endif
#ifndef NO_EC
    /* Setup our EC_KEY_METHOD that we provide pointers to */
    ec_key_meth     = EC_KEY_OpenSSL();
    pka_ec_key_meth = EC_KEY_METHOD_new(ec_key_meth);
    EC_KEY_METHOD_set_compute_key(pka_ec_key_meth, engine_pka_ecdh_compute_key);
    //EC_KEY_METHOD_set_init(pka_ec_key_meth, engine_pka_ec_init, engine_pka_ec_finish, NULL, NULL, NULL, NULL);
    EC_KEY_METHOD_set_sign(pka_ec_key_meth, engine_pka_ecdsa_sign, engine_pka_ecdsa_sign_setup, engine_pka_ecdsa_sign_sig);
    EC_KEY_METHOD_set_verify(pka_ec_key_meth, engine_pka_ecdsa_verify, engine_pka_ecdsa_verify_sig);

#endif
    /* Setup our RAND_METHOD that we provide pointers to */

# else /* OpenSSL >=1.0.0 && < 1.0.1 */
#ifndef NO_RSA
    /*
     * We know that the "RSA_PKCS1_SSLeay()" functions hook properly
     * to the bluefield-specific engine_pka_rsa_mod_exp so we use
     * those functions.
     */
    const RSA_METHOD *meth  = RSA_PKCS1_SSLeay();
    pka_rsa_meth.rsa_pub_enc = meth->rsa_pub_enc;
    pka_rsa_meth.rsa_pub_dec = meth->rsa_pub_dec;
    pka_rsa_meth.rsa_priv_enc = meth->rsa_priv_enc;
    pka_rsa_meth.rsa_priv_dec = meth->rsa_priv_dec;
#endif
#ifndef NO_DH
    /* Setup our DH_METHOD that we provide pointers to */
    /*
     * We know that the "DH_OpenSSL()" functions hook properly
     * to the bluefield-specific engine_pka_dh_bn_mod_exp so we use
     * those functions.
     */
    const DH_METHOD *dh_meth  = DH_OpenSSL();
    pka_dh_meth.generate_key = dh_meth->generate_key;
    pka_dh_meth.compute_key = dh_meth->compute_key;
#endif
#ifndef NO_DSA
    /*
     * We know that the "DSA_OpenSSL()" functions hook properly
     * to the bluefield-specific engine_pka_dsa_mod_exp and 
     * engine_pka_dsa_bn_mod_exp, so we use those functions.
     */
    const DSA_METHOD *dsa_meth = DSA_OpenSSL();
    pka_dsa_meth.dsa_do_sign = dsa_meth->dsa_do_sign;
    pka_dsa_meth.dsa_sign_setup = dsa_meth->dsa_sign_setup;
    pka_dsa_meth.dsa_do_verify = dsa_meth->dsa_do_verify;
#endif
#ifndef NO_EC
    /* Setup our EC_KEY_METHOD that we provide pointers to */
#endif
    /* Setup our RAND_METHOD that we provide pointers to */
# endif

    if (rc != ENGINE_set_id(e, engine_pka_id)
        || rc != ENGINE_set_name(e, engine_pka_name)
# if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#ifndef NO_RSA
        || rc != ENGINE_set_RSA(e, pka_rsa_meth)
#endif
#ifndef NO_DH
        || rc != ENGINE_set_DH(e, pka_dh_meth)
#endif
#ifndef NO_DSA
        || rc != ENGINE_set_DSA(e, pka_dsa_meth)
#endif
#ifndef NO_EC
        || rc != ENGINE_set_EC(e, pka_ec_key_meth)
#endif
# else /* OpenSSL >=1.0.0 && < 1.0.1 */
#ifndef NO_RSA
        || rc != ENGINE_set_RSA(e, &pka_rsa_meth)
#endif
#ifndef NO_DH
        || rc != ENGINE_set_DH(e, &pka_dh_meth)
#endif
#ifndef NO_DSA
        || rc != ENGINE_set_DSA(e, &pka_dsa_meth)
#endif
# endif
        || rc != ENGINE_set_destroy_function(e, engine_pka_destroy)
        || rc != ENGINE_set_init_function(e, engine_pka_init)
        || rc != ENGINE_set_finish_function(e, engine_pka_finish))
    {
        printf("ERROR: failed to setup ENGINE [%s] %s\n",
               engine_pka_id, engine_pka_name);
        return 0;
    }

    return 1;
}

static int bind_helper(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_pka_id) != 0))
        return 0;
    if (!bind_pka(e))
        return 0;
    return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)

static ENGINE *engine_pka(void)
{
    ENGINE *ret = ENGINE_new();
    if (!ret)
        return NULL;
    if (!bind_pka(ret)) {
        ENGINE_free(ret);
        return NULL;
    }
    return ret;
}

void engine_load_pka_int(void)
{
    ENGINE *toadd = engine_pka();
    if (!toadd)
        return;
    ENGINE_add(toadd);
    ENGINE_free(toadd);
}

static int engine_pka_init(ENGINE *e)
{
    return pka_init();
}

static int engine_pka_finish(ENGINE *e)
{
    return pka_finish();
}

static int engine_pka_destroy(ENGINE *e)
{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    RSA_meth_free(pka_rsa_meth);
    DH_meth_free(pka_dh_meth);
    DSA_meth_free(pka_dsa_meth);
    EC_KEY_METHOD_free(pka_ec_key_meth);
#endif
    return 1;
}

/* BN operations */

/* BN_mod_exp */
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int
engine_pka_bn_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                      const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    BIGNUM *result;
    int     rc, result_bit_len;

    rc = 0;

    result         = BN_new();
    result_bit_len = BN_num_bits(m);

    /* Expand the result bn so that it can hold our big num */
    if (!BN_lshift(result, result, result_bit_len))
    {
        printf("ERROR: bn_mod_exp failed to expand RSA result component\n");
        goto end;
    }

    rc = pka_rsa_mod_exp((pka_bignum_t *) a,
                         (pka_bignum_t *) p,
                         (pka_bignum_t *) m,
                         (pka_bignum_t *) result);

    if (!rc || BN_copy(r, result) == NULL)
        rc = 0;

end:
    BN_free(result);
    return rc;
}

static int
engine_pka_ecc_pt_add(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                      const EC_POINT *b, BN_CTX *ctx)
{
    printf("\n Inside pka point addition \n");
    if(r == NULL || a == NULL || b == NULL)
        return 0;

    BIGNUM *P  = BN_new(); 
    BIGNUM *A  = BN_new();
    BIGNUM *B  = BN_new();
    BIGNUM *X1 = BN_new(); 
    BIGNUM *Y1 = BN_new();
    BIGNUM *X2 = BN_new(); 
    BIGNUM *Y2 = BN_new();
    BIGNUM *XR = BN_new(); 
    BIGNUM *YR = BN_new();
    int     rc = 1;
    int     result_bit_len;

    if(!EC_GROUP_get_curve(group, P, A, B, ctx))
    {
        printf("\n ERROR: engine_pka_ecc_pt_add failed to get curve. \n");
        rc = 0;
        goto end;
    }

    result_bit_len = BN_num_bits(P);

    /* Preallocate result bn's so that it can hold our big num */
    if(!BN_set_bit(XR, result_bit_len)
    || !BN_set_bit(YR, result_bit_len))
    {
        printf("ERROR: engine_pka_ecc_pt_add failed to expand result component\n");
        goto end;
    }

    if(!EC_POINT_get_affine_coordinates(group, a, X1, Y1, ctx)
    || !EC_POINT_get_affine_coordinates(group, b, X2, Y2, ctx)
    || !EC_POINT_get_affine_coordinates(group, r, XR, YR, ctx))
    {
        printf("\n ERROR: engine_pka_ecc_pt_add failed to get point co-ordinates. \n");
        rc = 0;
        goto end;
    }

    if(!pka_ecc_bn_pt_add((pka_bignum_t *) P,
                          (pka_bignum_t *) A,
                          (pka_bignum_t *) B,
                          (pka_bignum_t *) X1,
                          (pka_bignum_t *) Y1,
                          (pka_bignum_t *) X2,
                          (pka_bignum_t *) Y2,
                          (pka_bignum_t *) XR,
                          (pka_bignum_t *) YR))
    {
        printf("\n ERROR: engine_pka_ecc_pt_add operation failed. \n");
        rc = 0;
        goto end;
    }

    if(!EC_POINT_set_affine_coordinates(group, r, XR, YR, ctx))
    {
        printf("\n ERROR: engine_pka_ecc_pt_add failed to set result point co-ordinates. \n");
        rc = 0;
        goto end;
    }

end:
    BN_free(P);
    BN_free(A);
    BN_free(B);
    BN_free(X1);
    BN_free(Y1);
    BN_free(X2);
    BN_free(Y2);
    BN_free(XR);
    BN_free(YR);
    return rc;
}

/* NOTE: OpenSSL library recommends that the engine_pka_ecc_pt_mult(),
 *       meet the below requirements:
 *
 *   r := generator * scalar
 *        + points[0] * scalars[0]
 *        + ...
 *        + points[num-1] * scalars[num-1].
 *
 * For a fixed point multiplication (scalar != NULL, num == 0)
 * or a variable point multiplication (scalar == NULL, num == 1),
 * mul() must use a constant time algorithm: in both cases callers
 * should provide an input scalar (either scalar or scalars[0])
 * in the range [0, ec_group_order); for robustness, implementers
 * should handle the case when the scalar has not been reduced, but
 * may treat it as an unusual input, without any constant-timeness
 * guarantee.
 */
static int
engine_pka_ecc_pt_mult(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
                       size_t num, const EC_POINT *points[],
                       const BIGNUM *scalars[], BN_CTX *ctx)
{
    int             rc, result_bit_len, print_val;
    EC_POINT       *result;
    const EC_POINT *g;
    BIGNUM         *P  = BN_new();
    BIGNUM         *A  = BN_new();
    BIGNUM         *B  = BN_new();
    BIGNUM         *X  = BN_new();
    BIGNUM         *Y  = BN_new();
    BIGNUM         *XR = BN_new();
    BIGNUM         *YR = BN_new();
    BIGNUM         *XD = NULL;
    BIGNUM         *YD = NULL;

    print_val          = 0;
    rc                 = 1;
    result             = EC_POINT_new(group);    
    g                  = EC_GROUP_get0_generator(group);
    
    if(EC_GROUP_get_curve_name(group) == 716)
        print_val = 1;    

    if(result == NULL)
        goto end;
 
    // Retrieve p,a,b parameters pertaining to the curve.
    if(!EC_GROUP_get_curve(group, P, A, B, ctx))
    {
        rc = 0;
        goto end;
    }

    result_bit_len = BN_num_bits(P);

    /* Preallocate result bn's so that it can hold our big num */
    if(!BN_lshift(XR, XR, result_bit_len)
    || !BN_lshift(YR, YR, result_bit_len))
    {
        printf("ERROR: engine_pka_ecc_pt_mult failed to expand result component\n");
        goto end;
    }

    // Generator point present.
    if(scalar != NULL && g != NULL)
    {
        if(!EC_POINT_get_affine_coordinates(group, g, X, Y, ctx))
        {
            printf("\n ERROR: engine_pka_ecc_pt_mult:"
                              "failed to get generator point co-ordinates. \n");
            rc = 0;
            goto end;
        }

        rc &= pka_ecc_bn_pt_mult((pka_bignum_t *) P,
                                 (pka_bignum_t *) A,
                                 (pka_bignum_t *) B,
                                 (pka_bignum_t *) X,
                                 (pka_bignum_t *) Y,
                                 (pka_bignum_t *) scalar,
                                 (pka_bignum_t *) XR,
                                 (pka_bignum_t *) YR,
                                 print_val);

        if(print_val){
            BIO *fptr;
            fptr = BIO_new_fp(stdout, BIO_NOCLOSE);
            BN_print(fptr, XR);
            BN_print(fptr, YR);
            BIO_free_all(fptr);
        }

        // Exit if operation failed.
        if(!rc)
            goto end;

        // Duplicate the result for future use.
        if(num >= 1)
        {
            XD = BN_dup(XR);
            YD = BN_dup(YR);
            if(XD == NULL || YD == NULL)
            {   
                rc = 0;
                goto end;
            }
        }
    }

    // Input points passed via points[] and scalar via scalars[].
    if(num >= 1 && points != NULL && scalars != NULL)
    {
	uint64_t  i;
        BIGNUM   *XP = NULL;
        BIGNUM   *YP = NULL;

        for(i = 0; i < num; i++)
        {
            // Input check.
            if(points[i] == NULL || scalars[i] == NULL)
            {
                rc = 0;
                goto end;
            }

            if(!EC_POINT_get_affine_coordinates(group, points[i], X, Y, ctx))
            {
                printf("\n ERROR: engine_pka_ecc_pt_mult:"
                                  "failed to get point co-ordinates. \n");
                rc = 0;
                goto end;
            }

            rc &= pka_ecc_bn_pt_mult((pka_bignum_t *) P,
                                     (pka_bignum_t *) A,
                                     (pka_bignum_t *) B,
                                     (pka_bignum_t *) X,
                                     (pka_bignum_t *) Y,
                                     (pka_bignum_t *) scalars[i],
                                     (pka_bignum_t *) XR,
                                     (pka_bignum_t *) YR,
                                     print_val);

            // Skip addition for the first iteration.
            if(rc && i != 0)
            {
                rc &= pka_ecc_bn_pt_add((pka_bignum_t *) P,
                                        (pka_bignum_t *) A,
                                        (pka_bignum_t *) B,
                                        (pka_bignum_t *) XR,
                                        (pka_bignum_t *) YR,
                                        (pka_bignum_t *) XP,
                                        (pka_bignum_t *) YP,
                                        (pka_bignum_t *) XR,
                                        (pka_bignum_t *) YR);
                BN_free(XP);
                BN_free(YP);
            }
            // Add the (generator*scalar) point if present, during the first iteration.
            else if(rc && scalar != NULL && g != NULL)
            {
                rc &= pka_ecc_bn_pt_add((pka_bignum_t *) P,
                                        (pka_bignum_t *) A,
                                        (pka_bignum_t *) B,
                                        (pka_bignum_t *) XR,
                                        (pka_bignum_t *) YR,
                                        (pka_bignum_t *) XD,
                                        (pka_bignum_t *) YD,
                                        (pka_bignum_t *) XR,
                                        (pka_bignum_t *) YR);
                BN_free(XD);
                BN_free(YD);
            }
            
            // Exit if any of the above operations failed.
            if(!rc)
                goto end;

            // Store the results for addition during next iteration.
            // Not required to store result for last iteration.
            if(i < num-1)
            {
                XP = BN_dup(XR);
                YP = BN_dup(YR);
                if(XP == NULL || YP == NULL)
                {   
                    rc = 0;
                    goto end;
                }
            }
        }
    }

    if(!EC_POINT_set_affine_coordinates(group, r, XR, YR, ctx))
    {
        printf("\nCurve: %d ", EC_GROUP_get_curve_name(group));    
        printf("ERROR: engine_pka_ecc_pt_mult points vector set:size=%ld\n",num);
        if(scalar != NULL && g != NULL)
            printf("ERROR: engine_pka_ecc_pt_mult generator point set\n");
        printf("\n ERROR: engine_pka_ecc_pt_mult:"
                         "failed to set result point co-ordinates. \n");
        rc = 0;
        goto end;
    }

end:
    BN_free(P);
    BN_free(A);
    BN_free(B);
    BN_free(X);
    BN_free(Y);
    BN_free(XR);
    BN_free(YR);
    if(result)
        EC_POINT_free(result);
    return rc;
}

#ifndef NO_RSA
/* RSA implementation */
static int
engine_pka_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
    int           rc = 0, result_bit_len;
    BIGNUM       *result;
# if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    const BIGNUM *n, *e, *d;
    const BIGNUM *p, *q, *dmp1, *dmq1, *iqmp;

    /* Do not check input parameters - ignore errors; we carry on anyway */
    RSA_get0_factors(rsa, &p, &q);
    RSA_get0_crt_params(rsa, &dmp1, &dmq1, &iqmp);
    RSA_get0_key(rsa, &n, &e, &d);

    if (n != NULL)
    {
        result         = BN_new();
        result_bit_len = BN_num_bits(n);
        /* Expand the result bn so that it can hold our big num */
        if (!BN_lshift(result, result, result_bit_len))
        {
            printf("ERROR: failed to expand RSA result component\n");
            goto end;
        }

        /* *TBD* Perform in software if modulus is too large for hardware. */

        /* See if we have all the necessary bits for a crt */
        if (p && q && dmp1 && dmq1 && iqmp) {
            rc = pka_rsa_mod_exp_crt((pka_bignum_t *) I,
                                     (pka_bignum_t *) p,
                                     (pka_bignum_t *) q,
                                     (pka_bignum_t *) dmp1,
                                     (pka_bignum_t *) dmq1,
                                     (pka_bignum_t *) iqmp,
                                     (pka_bignum_t *) result);
        }
        else if (d)
        {
            rc = pka_rsa_mod_exp((pka_bignum_t *) I,
                                 (pka_bignum_t *) d,
                                 (pka_bignum_t *) n,
                                 (pka_bignum_t *) result);
        }
# else
    if (rsa->n != NULL)
    {
        result         = BN_new();
        result_bit_len = BN_num_bits(rsa->n);
        /* Expand the result bn so that it can hold our big num */
        if (!BN_lshift(result, result, result_bit_len))
        {
            printf("ERROR: failed to expand RSA result component\n");
            goto end;
        }

        /* *TBD* Perform in software if modulus is too large for hardware. */

        /* See if we have all the necessary bits for a crt */
        if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
            rc = pka_rsa_mod_exp_crt((pka_bignum_t *) I,
                                     (pka_bignum_t *) rsa->p,
                                     (pka_bignum_t *) rsa->q,
                                     (pka_bignum_t *) rsa->dmp1,
                                     (pka_bignum_t *) rsa->dmq1,
                                     (pka_bignum_t *) rsa->iqmp,
                                     (pka_bignum_t *) result);
        }
        else if (rsa->d)
        {
            rc = pka_rsa_mod_exp((pka_bignum_t *) I,
                                 (pka_bignum_t *) rsa->d,
                                 (pka_bignum_t *) rsa->n,
                                 (pka_bignum_t *) result);
        }
# endif
        else
        {
            printf("ERROR: RSA missing key components\n");
            goto end;
        }

        if (rc && BN_copy(r0, result) != NULL)
            goto end;
    }
    else
    {
        printf("ERROR: RSA missing modulus component\n");
        return 0;
    }

end:
    BN_free(result);
    return rc;
}

static int engine_pka_rsa_init(RSA *rsa)
{
    return pka_init();
}

static int engine_pka_rsa_finish(RSA *rsa)
{
    return pka_finish();
}
#endif

#ifndef NO_DH
/* DH implementation */
static int
engine_pka_dh_bn_mod_exp(const DH *dh, BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return engine_pka_bn_mod_exp(r, a, p, m, ctx, m_ctx);
}

static int engine_pka_dh_init(DH *dh)
{
    return pka_init();
}

static int engine_pka_dh_finish(DH *dh)
{
    return pka_finish();
}
#endif
#ifndef NO_DSA
/* DSA implementation */
static int
engine_pka_dsa_bn_mod_exp(DSA *dsa, BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                          const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return engine_pka_bn_mod_exp(r, a, p, m, ctx, m_ctx);
}

static int engine_pka_dsa_mod_exp(DSA *dsa, BIGNUM *rr, const BIGNUM *a1,
                                  const BIGNUM *p1, const BIGNUM *a2,
                                  const BIGNUM *p2, const BIGNUM *m,
                                  BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
    // Algorithm: rr = a1^p1 * a2^p2 mod m
    BIGNUM *t;
    int rc;
    
    rc = 1;
    t  = BN_new();

    // t = a1^p1 mod m*/
    // rr = a2^p2 mod m */
    // rr = rr * t mod m */
    // TBD: BN_mod_mul() below can be replaced with pka library functions,
    //       it has to be checked whether this will improve the performance
    //       or the overhead of converting BN to pka_operand_t and vice versa
    //       will be detrimental to the performance. Also, it has to be investigated if
    //       the conversion BN <-> pka_operand_t can be skipped for intermediate results.
    // 
    if(!engine_pka_bn_mod_exp(t, a1, p1, m, ctx, in_mont)
       || !engine_pka_bn_mod_exp(rr, a2, p2, m, ctx, in_mont)
       || !BN_mod_mul(rr,rr,t,m,ctx))
        rc = 0;

    BN_free(t);
    return rc; 
}

static int engine_pka_dsa_init(DSA *dsa)
{
    return pka_init();
}

static int engine_pka_dsa_finish(DSA *dsa)
{
    return pka_finish();
}
#endif
#ifndef NO_EC
/*
static int engine_pka_ec_init(EC_KEY *key)
{
    printf("\n ECDSA: init: \n");
    return pka_init();
}

static void engine_pka_ec_finish(EC_KEY *key)
{
    printf("\n ECDSA: finish: \n");
    pka_finish();
}
*/
static int override_pka_mul_add(const EC_KEY *eckey, EC_GROUP **dup_group_ptr, EC_KEY **dup_key_ptr)
{
    int             rc;
    const EC_GROUP *group;
    EC_GROUP       *dup_group;
    EC_KEY         *dup_key; 

    rc    = 1;
    group = EC_KEY_get0_group(eckey);

    // Duplicate group and ecdh in order to override functions.
    dup_group = EC_GROUP_dup(group);
    dup_key   = EC_KEY_dup(eckey);

    if(!dup_group || !dup_key)
    {
        printf("\n ERROR: Duplication failed \n");    
        rc = 0;
        goto end;
    }

    if(dup_group->meth == NULL)
    {   // Create EC_METHOD struct so as to set our engine functions. 
        dup_group->meth = malloc(sizeof(EC_METHOD));
    }

    // Override the mul and add functions with our engine functions.
    dup_group->meth->mul  = engine_pka_ecc_pt_mult;
    dup_group->meth->add  = engine_pka_ecc_pt_add;

    // Set the structs with our overrided functions.
    if(!EC_KEY_set_group(dup_key, dup_group))
    {
        printf("\n ERROR: Failed to set droup \n");
        rc = 0;
        goto end;
    }

    // Set the output
    *dup_group_ptr = dup_group;
    *dup_key_ptr   = dup_key;
end:
    return rc;
}

static int
engine_pka_ecdh_compute_key(unsigned char **pout, size_t *poutlen,
                       const EC_POINT *pub_key, const EC_KEY *ecdh)
{
    int             rc;
    const EC_GROUP *group;
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;

    // Function pointer to hold the OpenSSL ECDH compute function. 
    int (*compute_key)(unsigned char **pout, size_t *poutlen,
                       const EC_POINT *pub_key, const EC_KEY *ecdh);
   
    group     = EC_KEY_get0_group(ecdh);

    // Duplicate group and ecdh in order to override functions.
    dup_group = EC_GROUP_dup(group);
    dup_key   = EC_KEY_dup(ecdh);

    if(dup_group == NULL || dup_key == NULL)
    {
        rc = 0;
        goto end;
    }

    if(dup_group->meth == NULL)
    {   // Create EC_METHOD struct so as to set our engine functions. 
        dup_group->meth = malloc(sizeof(EC_METHOD));
    }

    // Override the mul and add functions with our engine functions.
    dup_group->meth->mul  = engine_pka_ecc_pt_mult;
    dup_group->meth->add  = engine_pka_ecc_pt_add;

    // Set the structs with our overrided functions.
    EC_KEY_set_group(dup_key, dup_group);

    // Get the OpenSSL ECDH compute key function.
    EC_KEY_METHOD_get_compute_key(ec_key_meth, &compute_key);

    // Perform the computation with mul and add overriden to our engine functions.
    rc = compute_key(pout, poutlen, pub_key, dup_key);

    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return rc;
}

static int
engine_pka_ecdsa_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
                      unsigned int *siglen, const BIGNUM *kinv, const BIGNUM *r, EC_KEY *eckey)
{
    int             rc;
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;

    int (*sign)(int type, const unsigned char *dgst, int dlen, unsigned char
                *sig, unsigned int *siglen, const BIGNUM *kinv,
                const BIGNUM *r, EC_KEY *eckey);

    int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                      BIGNUM **rp);

    ECDSA_SIG *(*sign_sig)(const unsigned char *dgst, int dgst_len,
                           const BIGNUM *in_kinv, const BIGNUM *in_r,
                           EC_KEY *eckey);

    if(!override_pka_mul_add(eckey, &dup_group, &dup_key))
    {
        rc = 0;
        goto end;
    }
    
    EC_KEY_METHOD_get_sign(ec_key_meth, &sign, &sign_setup, &sign_sig);
    
    rc = sign(type, dgst, dlen, sig, siglen, kinv, r, dup_key);
    
    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return rc;
}

static int engine_pka_ecdsa_sign_setup(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                                      BIGNUM **rp)
{
    int             rc;
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;

    int (*sign)(int type, const unsigned char *dgst, int dlen, unsigned char
                *sig, unsigned int *siglen, const BIGNUM *kinv,
                const BIGNUM *r, EC_KEY *eckey);

    int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                      BIGNUM **rp);

    ECDSA_SIG *(*sign_sig)(const unsigned char *dgst, int dgst_len,
                           const BIGNUM *in_kinv, const BIGNUM *in_r,
                           EC_KEY *eckey);

    if(!override_pka_mul_add(eckey, &dup_group, &dup_key))
    {
        rc = 0;
        goto end;
    }
    
    EC_KEY_METHOD_get_sign(ec_key_meth, &sign, &sign_setup, &sign_sig);
    
    rc = sign_setup(dup_key, ctx_in, kinvp, rp);
    
    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return rc;
}

static ECDSA_SIG*
engine_pka_ecdsa_sign_sig(const unsigned char *dgst, int dgst_len,
                          const BIGNUM *in_kinv, const BIGNUM *in_r,
                          EC_KEY *eckey)
{
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;
    ECDSA_SIG      *ret       = NULL;

    int (*sign)(int type, const unsigned char *dgst, int dlen, unsigned char
                *sig, unsigned int *siglen, const BIGNUM *kinv,
                const BIGNUM *r, EC_KEY *eckey);

    int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
                      BIGNUM **rp);

    ECDSA_SIG *(*sign_sig)(const unsigned char *dgst, int dgst_len,
                           const BIGNUM *in_kinv, const BIGNUM *in_r,
                           EC_KEY *eckey);
    if(!override_pka_mul_add(eckey, &dup_group, &dup_key))
    {
        goto end;
    }
    
    EC_KEY_METHOD_get_sign(ec_key_meth, &sign, &sign_setup, &sign_sig);
    
    ret = sign_sig(dgst, dgst_len, in_kinv, in_r, dup_key);
    
    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return ret;
}

static int
engine_pka_ecdsa_verify(int type, const unsigned char *dgst, int dgst_len,
                        const unsigned char *sigbuf, int sig_len, EC_KEY *eckey)
{
    int             rc;
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;

    int (*verify)(int type, const unsigned char *dgst, int dgst_len,
                  const unsigned char *sigbuf, int sig_len, EC_KEY *eckey);

    int (*verify_sig)(const unsigned char *dgst, int dgst_len,
                      const ECDSA_SIG *sig, EC_KEY *eckey);

    if(!override_pka_mul_add(eckey, &dup_group, &dup_key))
    {
        rc = 0;
        goto end;
    }
    
    EC_KEY_METHOD_get_verify(ec_key_meth, &verify, &verify_sig);
    
    rc = verify(type, dgst, dgst_len, sigbuf, sig_len, dup_key);
    
    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return rc;
}

static int
engine_pka_ecdsa_verify_sig(const unsigned char *dgst, int dgst_len,
                            const ECDSA_SIG *sig, EC_KEY *eckey)
{
    int             rc;
    EC_GROUP       *dup_group = NULL;
    EC_KEY         *dup_key   = NULL;

    int (*verify)(int type, const unsigned char *dgst, int dgst_len,
                  const unsigned char *sigbuf, int sig_len, EC_KEY *eckey);

    int (*verify_sig)(const unsigned char *dgst, int dgst_len,
                      const ECDSA_SIG *sig, EC_KEY *eckey);

    if(!override_pka_mul_add(eckey, &dup_group, &dup_key))
    {
        rc = 0;
        goto end;
    }
    
    EC_KEY_METHOD_get_verify(ec_key_meth, &verify, &verify_sig);
    
    rc = verify_sig(dgst, dgst_len, sig, dup_key);
    
    EC_GROUP_free(dup_group);
    EC_KEY_free(dup_key);
end:
    return rc;
}

#endif
#endif /* BLUEFIELD_DYNAMIC_ENGINE */
