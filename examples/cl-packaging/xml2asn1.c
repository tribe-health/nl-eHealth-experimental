/**
 * Based on http://www.xmlsoft.org/examples/tree1.c
 */
#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openssl/conf.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/bn.h>
#include <openssl/err.h>

/** Structure in XML:

IssuerPublicKey
   <Counter>0</Counter>
   <ExpiryDate>1643236269</ExpiryDate>
   <Elements>
      <n>20...
      <Z>2065...
      <S>974702....
      <Bases num="N">
         <Base_0>1378....
         <Base_1>122...
         ...
         <Base_(N-1)>122...
      </Bases>
   <Features>
      <Epoch length="432000"></Epoch>

Enterprise OID: 1847 (jrc, European Commision)

ASN1_ITEM_TEMPLATE(ZKP_PUBKEY_FEATURE) =
        ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, ZKP_PUBKEY_FEATURE, ASN1_INTEGER)

static_ASN1_ITEM_TEMPLATE_END(ZKP_PUBKEY_FEATURE)

IMPLEMENT_ASN1_ALLOC_FUNCTIONS(ZKP_PUBKEY_FEATURE)

const X509V3_EXT_METHOD v3_zkp_feature = {
    NID_zkp, 0,
    ASN1_ITEM_ref(ZKP_PUBKEY_FEATURE),
    0, 0, 0, 0,
    0, 0,
    (X509V3_EXT_I2V)i2v_ZKP_PUBKEY_FEATURE,
    (X509V3_EXT_V2I)v2i_ZKP_PUBKEY_FEATURE,
    0, 0,
    NULL
};
*/

#define NID_ENTERPRISE               1.3.6.1.4.1.
#define NID_JOINT_RESEARCH_CENTRE_EU NID_ENTERPRISE.1847
#define NID_EHEALTH                  NID_JOINT_RESEARCH_CENTRE_EU.2021
#define NID_ZKP_PUBLIC_KEY           NID_EHEALTH.1
#define NID_ZKP_ALG                  NID_EHEALTH.2
#define NID_CL_ALG                   NID_ZKP_ALG.1
#define NID_BBSPLUS_ALG              NID_ZKP_ALG.2

#define xstr(s) str(s)
#define str(s) #s

#define OID_STR_ZKP xstr(NID_ZKP_PUBLIC_KEY)
#define OID_STR_CL xstr(NID_CL_ALG)

DEFINE_STACK_OF(BIGNUM)

typedef struct {
	ASN1_OBJECT    *algorithm;
	ASN1_INTEGER   *counter;
	BIGNUM         *n, *Z, *S;
	STACK_OF      (BIGNUM) * bases;
} ZKP_PUBLIC_KEY;

ASN1_SEQUENCE(ZKP_PUBLIC_KEY) =
{
	ASN1_SIMPLE(ZKP_PUBLIC_KEY, algorithm, ASN1_OBJECT),
	ASN1_SIMPLE(ZKP_PUBLIC_KEY, counter, ASN1_INTEGER),
	ASN1_SIMPLE(ZKP_PUBLIC_KEY, n, BIGNUM),
	ASN1_SIMPLE(ZKP_PUBLIC_KEY, Z, BIGNUM),
	ASN1_SIMPLE(ZKP_PUBLIC_KEY, S, BIGNUM),
	ASN1_SEQUENCE_OF(ZKP_PUBLIC_KEY, bases, BIGNUM)
} ASN1_SEQUENCE_END(ZKP_PUBLIC_KEY)

IMPLEMENT_ASN1_FUNCTIONS(ZKP_PUBLIC_KEY)

static void
print_element_names(char *at, xmlNode * a_node)
{
	char		buff      [1024], *p;
	xmlNode        *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		p = at;
		if (cur_node->type == XML_ELEMENT_NODE) {
			snprintf(buff, sizeof(buff), "%s/%s", at, cur_node->name);
			p = buff;
		};
		if (cur_node->type == XML_TEXT_NODE)
			printf("%s\n\t<%s>\n", buff, cur_node->content);
		print_element_names(buff, cur_node->children);
	}
}

static char    *
_get_element(char *path, char *at, xmlNode * a_node)
{
	char		buff      [1024], *p;
	xmlNode        *cur_node = NULL;
	char           *out = NULL;

	for (cur_node = a_node; cur_node && out == NULL; cur_node = cur_node->next) {
		p = at;
		if (cur_node->type == XML_ELEMENT_NODE) {
			snprintf(buff, sizeof(buff), "%s/%s", at, cur_node->name);
			p = buff;
		};
		if (cur_node->type == XML_TEXT_NODE)
			if (!strcmp(path, p))
				return (char *)cur_node->content;
		out = _get_element(path, p, cur_node->children);
	}
	return out;
}
static char    *
get_element(char *path, xmlNode * a_node)
{
	return _get_element(path, "", a_node);
}

static ASN1_INTEGER *
get_ASN1_INTEGER(char *path, xmlNode * a_node)
{
	ASN1_INTEGER   *out = ASN1_INTEGER_new();
	char           *str;

	if (NULL == (str = get_element(path, a_node)))
		return NULL;

	if (0 == ASN1_INTEGER_set_uint64(out, strtoull(str, NULL, 10)))
		return NULL;

	return out;
};

static BIGNUM  *
get_BIGNUM(char *path, xmlNode * a_node)
{
	BIGNUM         *bn = NULL;
	char           *str;

	if (NULL == (str = get_element(path, a_node)))
		return NULL;

	if (0 == BN_dec2bn(&bn, (const char *)str))
		return NULL;

	return bn;
};

/**
 * Simple example to parse a file called "file.xml",
 * walk down the DOM, and print the name of the
 * xml elements nodes.
 */
int
main(int argc, char **argv)
{
	xmlDoc         *doc = NULL;
	xmlNode        *root_element = NULL;
	int		noout = 0,	der = 0, ascii = 0, cnf = 1, debug = 0;

	LIBXML_TEST_VERSION

	int		nid_zkp = OBJ_create(OID_STR_ZKP, "ZKP", "ZKP public key");
	int		nid_cl = OBJ_create(OID_STR_CL, "CL", "Camenisch-Lysyanskaya");

	{
		int	i;
		for (i = 1; i < argc && argv[i][0] == '-'; i++) {
			char		c = argv[i][1];
			switch (c) {
			case 'D':
				debug = 1;
			case 'd':
				der = 1;
			case 'n':
				noout = 1;
			case 'p':
				ascii = 1;
			case 'c':
				cnf = 1;
				break;
			case '?':
			case 'h':
			default:
				i = 100;
				break;

			};
		}

		if (i != argc - 1) {
			fprintf(stderr, "Syntax: %s [-D][-d][-n][-p[-c] <infile.xml>\n\tOutput goes to stdout.\n", argv[0]);
			return (1);
		}
		doc = xmlReadFile(argv[i], NULL, 0);
	}

	if (doc == NULL) {
		fprintf(stderr, "error: could not parse file '%s'\n", argv[1]);
		return (1);
	}
	root_element = xmlDocGetRootElement(doc);

	if (debug)
		print_element_names("", root_element);


	ZKP_PUBLIC_KEY *zkp = ZKP_PUBLIC_KEY_new();
	zkp->algorithm = OBJ_nid2obj(nid_cl);

#define GET_OR_EXIT(zkp, err, tpe, member, elem, root_element) { \
    if (err == (zkp->member = get_##tpe(elem, root_element))) { \
	fprintf(stderr,"Failed to convert " #elem " for " #member "\n"); \
	return(1); \
    }; \
}
	GET_OR_EXIT(zkp, NULL, ASN1_INTEGER, counter, "/IssuerPublicKey/Counter", root_element);
	GET_OR_EXIT(zkp, NULL, BIGNUM, n, "/IssuerPublicKey/Elements/n", root_element);
	GET_OR_EXIT(zkp, NULL, BIGNUM, Z, "/IssuerPublicKey/Elements/Z", root_element);
	GET_OR_EXIT(zkp, NULL, BIGNUM, S, "/IssuerPublicKey/Elements/S", root_element);

	for (int i = 0;; i++) {
		char		elem      [1024];
		BIGNUM         *bn = NULL;
		snprintf(elem, sizeof(elem), "/IssuerPublicKey/Elements/Bases/Base_%d", i);
		if (NULL == (bn = get_BIGNUM(elem, root_element)))
			break;
		sk_BIGNUM_push(zkp->bases, bn);
	};

	unsigned char	outbuff[1024 * 32], *p = outbuff;
	if (0 == i2d_ZKP_PUBLIC_KEY(zkp, &p)) {
		fprintf(stderr, "Failed to serialize.\n");
		return (1);
	};
	fprintf(stderr, "DER: %lu bytes\n", p - outbuff);
	if (der)
		fwrite(outbuff, p - outbuff, 1, stdout);
	else if (ascii) {
		BIO            *bio_out = BIO_new_fp(stdout, 0);

		if (!ASN1_parse_dump(bio_out, outbuff, p - outbuff, 4 /* indent */ , 1 /* unk as hex */ ))
			ERR_print_errors_fp(stderr);
		BIO_free_all(bio_out);
	} else if (cnf) {
		printf("# %s extension\n[zkp]\n%s=DER:", OBJ_nid2ln(nid_zkp), OID_STR_ZKP);
		for (unsigned char *q = outbuff; q < p; q++)
			printf("%02X", *q);
		printf("\n");
	} else if (!noout) {
		BIO            *bio_out = BIO_new_fp(stdout, 0);
		BIO            *bio_filter_b64 = BIO_new(BIO_f_base64());
		BIO_push(bio_filter_b64, bio_out);

		BIO_puts(bio_out, "-----BEGIN CL PUBLIC KEY-----\n");

		BIO_write(bio_filter_b64, outbuff, p - outbuff);
		BIO_flush(bio_filter_b64);

		BIO_puts(bio_out, "-----END CL PUBLIC KEY-----\n");

		BIO_free_all(bio_filter_b64);
	}
	ZKP_PUBLIC_KEY_free(zkp);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}
