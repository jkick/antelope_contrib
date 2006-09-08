/*
 * Antelope Orb interface for PHP
 *
 * Copyright (c) 2005 Lindquist Consulting, Inc.
 * All rights reserved. 
 *                                                                     
 * Written by Dr. Kent Lindquist, Lindquist Consulting, Inc. 
 * 
 * This software may be used freely in any way as long as 
 * the copyright statement above is not removed. 
 * 
 */

#include "Orb_php.h"
#include "stock.h"
#include "orb.h"
#include "Pkt.h"
#include "pf.h"
#include "pforbstat.h"

/* Prevent the deviants.h reassignment to std_now() from corrupting
 * the name of the PHP function */
#undef now

static int le_Orb;

static char *Elog_replacement = 0;

static function_entry Orb_functions[] = {
	PHP_FE(orbopen, NULL)		
	PHP_FE(orbping, NULL)		
	PHP_FE(orbtell, NULL)		
	PHP_FE(orbposition, NULL)		
	PHP_FE(orbafter, NULL)		
	PHP_FE(orbseek, NULL)		
	PHP_FE(orbwait, NULL)		
	PHP_FE(orbclose, NULL)		
	PHP_FE(orbselect, NULL)		
	PHP_FE(orbreject, NULL)		
	PHP_FE(orbreap, NULL)		
	PHP_FE(orbreap_nd, NULL)		
	PHP_FE(orbreap_timeout, NULL)		
	PHP_FE(orbget, NULL)		
	PHP_FE(orbput, NULL)		
	PHP_FE(orbputx, NULL)		
	PHP_FE(pforbstat, NULL)		
	PHP_FE(split_srcname, NULL)		
	PHP_FE(unstuffPkt, NULL)		
	{NULL, NULL, NULL}	
};

zend_module_entry Orb_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_ORB_EXTNAME,
	Orb_functions,
	PHP_MINIT(Orb),
	PHP_MSHUTDOWN(Orb),
	NULL,
	NULL,
	PHP_MINFO(Orb),
	PHP_ORB_EXTVER,
	STANDARD_MODULE_PROPERTIES
};

zend_object_value orb_pkt_obj_new( zend_class_entry *class_type TSRMLS_DC );

static zend_object_handlers orb_pkt_obj_handlers;

typedef struct _php_orb_pkt_obj {
	zend_object	std;
	int		type;
	Packet		*pkt;
} php_orb_pkt_obj;

PHP_METHOD(orb_pkt, PacketType);

zend_class_entry *php_orb_pkt_entry;
#define PHP_ORB_PKT_NAME "orb_pkt"
static function_entry php_orb_pkt_functions[] = {
	PHP_ME(orb_pkt, PacketType, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

ZEND_GET_MODULE(Orb)

void register_Orb_constants( INIT_FUNC_ARGS )
{
	int	i;

	for( i = 0; i < Orbxlatn; i++ ) {

		zend_register_long_constant( Orbxlat[i].name,
					     strlen( Orbxlat[i].name ) + 1, 
					     Orbxlat[i].num,
					     CONST_CS | CONST_PERSISTENT,
					     module_number TSRMLS_CC );
	}

	for( i = 0; i < Orbconstn; i++ ) {

		zend_register_long_constant( Orbconst[i].name,
					     strlen( Orbconst[i].name ) + 1, 
					     Orbconst[i].num,
					     CONST_CS | CONST_PERSISTENT,
					     module_number TSRMLS_CC );
	}

	for( i = 0; i < Pktxlatn; i++ ) {

		zend_register_long_constant( Pktxlat[i].name,
					     strlen( Pktxlat[i].name ) + 1, 
					     Pktxlat[i].num,
					     CONST_CS | CONST_PERSISTENT,
					     module_number TSRMLS_CC );
	}
}

PHP_MINIT_FUNCTION(Orb)
{
	zend_class_entry ce;

	register_Orb_constants( INIT_FUNC_ARGS_PASSTHRU );

	REGISTER_LONG_CONSTANT( "PFORBSTAT_SERVER", 
				 PFORBSTAT_SERVER, 
				 CONST_CS | CONST_PERSISTENT );
				 
	REGISTER_LONG_CONSTANT( "PFORBSTAT_SOURCES", 
				 PFORBSTAT_SOURCES, 
				 CONST_CS | CONST_PERSISTENT );

	REGISTER_LONG_CONSTANT( "PFORBSTAT_CLIENTS", 
				 PFORBSTAT_CLIENTS, 
				 CONST_CS | CONST_PERSISTENT );

	REGISTER_LONG_CONSTANT( "PFORBSTAT_DATABASES", 
				 PFORBSTAT_DATABASES, 
				 CONST_CS | CONST_PERSISTENT );

	REGISTER_LONG_CONSTANT( "PFORBSTAT_CONNECTIONS", 
				 PFORBSTAT_CONNECTIONS, 
				 CONST_CS | CONST_PERSISTENT );

	memcpy( &orb_pkt_obj_handlers, 
		zend_get_std_object_handlers(),
		sizeof( zend_object_handlers ) );

	INIT_CLASS_ENTRY( ce, PHP_ORB_PKT_NAME, php_orb_pkt_functions );	
	ce.create_object = orb_pkt_obj_new;
	php_orb_pkt_entry = zend_register_internal_class( &ce TSRMLS_CC );

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(Orb)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(Orb)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Orb support", "enabled");
	php_info_print_table_end();
}

static int
pf2zval( Pf *pf, zval *result ) {
	Pf	*pfvalue;
	int	ivalue;
	int	retcode = 0;
	zval	*element;
	Tbl	*keys;
	char	*key;

	switch( pf->type ) {
	case PFSTRING:

		ZVAL_STRING( result, pfexpand( pf ), 1 );
		break;

	case PFTBL:

		array_init( result );

		for( ivalue = 0; ivalue < maxtbl( pf->value.tbl ); ivalue++ ) {

			pfvalue = (Pf *) gettbl( pf->value.tbl, ivalue );

			MAKE_STD_ZVAL( element );

			pf2zval( pfvalue, element );

			add_index_zval( result, ivalue, element );
		}

		break;

	case PFFILE:
	case PFARR:

		keys = keysarr( pf->value.arr );

		array_init( result );

		for( ivalue = 0; ivalue < maxtbl( keys ); ivalue++ ) {

			key = gettbl( keys, ivalue );

			pfvalue = (Pf *) getarr( pf->value.arr, key );

			MAKE_STD_ZVAL( element );

			pf2zval( pfvalue, element );

			add_assoc_zval( result, key, element );
		}

		break;

	case PFINVALID:
	default:
		retcode = 1;
		break;
	}

	return retcode;
}

static zval *
pkt2zval( int type, Packet *pkt )
{
	zval	*zval_pkt;
	zend_class_entry *ce;
	php_orb_pkt_obj *intern;

	MAKE_STD_ZVAL( zval_pkt );

	ce = zend_fetch_class( PHP_ORB_PKT_NAME, 
			       strlen( PHP_ORB_PKT_NAME ), 
			       ZEND_FETCH_CLASS_AUTO );

	object_init_ex( zval_pkt, ce );

	intern = (php_orb_pkt_obj *) 
			zend_objects_get_address( zval_pkt TSRMLS_CC );

	intern->type = type;
	intern->pkt = pkt;

	return zval_pkt;
}

void
orb_pkt_obj_clone( void *object, void **object_clone TSRMLS_DC )
{
	; /* SCAFFOLD */

	return;
}

void 
orb_pkt_obj_free_resources( php_orb_pkt_obj *intern )
{
	if( intern ) {

		if( intern->pkt != (Packet *) NULL ) {
		
			freePkt( intern->pkt );
			intern->pkt = NULL;
		}
	}

	return;
}

void
orb_pkt_obj_free_storage( void *object TSRMLS_DC )
{
	php_orb_pkt_obj *intern = (php_orb_pkt_obj *) object;

	zend_hash_destroy( intern->std.properties );
	FREE_HASHTABLE( intern->std.properties );

	orb_pkt_obj_free_resources( intern );

	efree( object );
}

zend_object_value
orb_pkt_obj_new( zend_class_entry *class_type TSRMLS_DC )
{
	zend_object_value retval;
	php_orb_pkt_obj	*intern;
	zval	*tmp;

	intern = emalloc( sizeof( php_orb_pkt_obj ) );
	intern->std.ce = class_type;
	intern->std.guards = NULL;

	ALLOC_HASHTABLE( intern->std.properties );
	zend_hash_init( intern->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0 );
	zend_hash_copy( intern->std.properties, 
			&class_type->default_properties,
			(copy_ctor_func_t) zval_add_ref,
			(void *) &tmp,
			sizeof(zval *));
	retval.handle = zend_objects_store_put( intern, 
		(zend_objects_store_dtor_t) zend_objects_destroy_object, 
		(zend_objects_free_object_storage_t) orb_pkt_obj_free_storage,
		orb_pkt_obj_clone TSRMLS_CC);
	retval.handlers = &orb_pkt_obj_handlers;

	return retval;
}

/* {{{ proto array template( array db, ... ) *
PHP_FUNCTION(template)
{
	zval	*db_array;
	Dbptr	db;
	int	argc = ZEND_NUM_ARGS();

	if( argc != X ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "a", 
					&db_array )
	    == FAILURE) {

		return;

	} else if( z_arrval_to_dbptr( db_array, &db ) < 0 ) {

		return;
	}
}
/* }}} */

/* {{{ proto int orbopen( string name, string perm ) */
PHP_FUNCTION(orbopen)
{
	char	*orbname;
	int	orbname_len;
	char	*perm;
	int	perm_len;
	int	orbfd;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ss", 
					&orbname, &orbname_len,
					&perm, &perm_len )
	    == FAILURE) {

		return;
	}

	orbfd = orbopen( orbname, perm );

	RETURN_LONG( orbfd );
}
/* }}} */

/* {{{ proto int orbclose( int orbfd ) */
PHP_FUNCTION(orbclose)
{
	long	orbfd;
	int	rc;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "l", &orbfd )
	    == FAILURE) {

		return;
	}
	
	rc = orbclose( (int) orbfd );

	RETURN_LONG( rc );
}
/* }}} */

/* {{{ proto array orbreap( int orbfd ) */
PHP_FUNCTION(orbreap)
{
	long	orbfd;
	int	pktid;
	char	srcname[STRSZ];
	double	pkttime;
	char *pkt = 0;
	int	bufsize = 0;
	int	nbytes = 0;
	int	rc;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "l", &orbfd )
	    == FAILURE) {

		return;
	}
	
	rc = orbreap( (int) orbfd, &pktid, srcname, &pkttime, 
		      &pkt, &nbytes, &bufsize );

	if( rc < 0 ) {
		
		zend_error( E_ERROR, "orbreap failed" );

		return;
	}

	array_init( return_value );

	add_next_index_long( return_value, pktid );
	add_next_index_string( return_value, srcname, 1 );
	add_next_index_double( return_value, pkttime );
	add_next_index_stringl( return_value, pkt, (uint) nbytes, 1 );
	add_next_index_long( return_value, nbytes );

	if( pkt != 0 ) {

		free( pkt );
	}

	return;
}
/* }}} */

/* {{{ proto array orbreap_nd( int orbfd ) */
PHP_FUNCTION(orbreap_nd)
{
	long	orbfd;
	int	pktid;
	char	srcname[STRSZ];
	double	pkttime;
	char *pkt = 0;
	int	bufsize = 0;
	int	nbytes = 0;
	int	rc;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "l", &orbfd )
	    == FAILURE) {

		return;
	}
	
	rc = orbreap_nd( (int) orbfd, &pktid, srcname, &pkttime, 
		      &pkt, &nbytes, &bufsize );

	if( rc < 0 ) {
		
		return;
	}

	array_init( return_value );

	add_next_index_long( return_value, pktid );
	add_next_index_string( return_value, srcname, 1 );
	add_next_index_double( return_value, pkttime );
	add_next_index_stringl( return_value, pkt, (uint) nbytes, 1 );
	add_next_index_long( return_value, nbytes );

	if( pkt != 0 ) {

		free( pkt );
	}

	return;
}
/* }}} */

/* {{{ proto array orbreap_timeout( int orbfd, int maxseconds ) */
PHP_FUNCTION(orbreap_timeout)
{
	long	orbfd;
	long	maxseconds;
	int	pktid;
	char	srcname[STRSZ];
	double	pkttime;
	char *pkt = 0;
	int	bufsize = 0;
	int	nbytes = 0;
	int	rc;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ll", &orbfd, &maxseconds )
	    == FAILURE) {

		return;
	}
	
	rc = orbreap_timeout( (int) orbfd, (int) maxseconds, &pktid, 
			srcname, &pkttime, &pkt, &nbytes, &bufsize );

	if( rc < 0 ) {
		
		return;
	}

	array_init( return_value );

	add_next_index_long( return_value, pktid );
	add_next_index_string( return_value, srcname, 1 );
	add_next_index_double( return_value, pkttime );
	add_next_index_stringl( return_value, pkt, (uint) nbytes, 1 );
	add_next_index_long( return_value, nbytes );

	if( pkt != 0 ) {

		free( pkt );
	}

	return;
}
/* }}} */

/* {{{ proto array orbget( int orbfd, mixed which ) */
PHP_FUNCTION(orbget)
{
	long	orbfd;
	int	pktid;
	char	srcname[STRSZ];
	double	pkttime;
	char *pkt = 0;
	int	bufsize = 0;
	int	nbytes = 0;
	int	rc;
	zval	*zval_which;
	int	which;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "lz", &orbfd, &zval_which )
	    == FAILURE) {

		return;
	}
	
	if( Z_TYPE_P( zval_which ) == IS_STRING ) {

		which = xlatname( Z_STRVAL_P( zval_which ), Orbconst, Orbconstn );

		if( which == -1 ) {

			zend_error( E_ERROR, 
		     	   "orbget 'which' code not recognized" );

			return;
		}
	
	} else if( Z_TYPE_P( zval_which ) == IS_LONG ) {

		which = Z_LVAL_P( zval_which );

	} else {

		zend_error( E_ERROR, 
		     "orget 'which' argument must be string or integer" );

		return;
	}

	rc = orbget( (int) orbfd, which, &pktid, srcname, &pkttime, 
		      &pkt, &nbytes, &bufsize );

	if( rc < 0 ) {
		
		zend_error( E_ERROR, "orbget failed" );

		return;
	}

	array_init( return_value );

	add_next_index_long( return_value, pktid );
	add_next_index_string( return_value, srcname, 1 );
	add_next_index_double( return_value, pkttime );
	add_next_index_stringl( return_value, pkt, (uint) nbytes, 1 );
	add_next_index_long( return_value, nbytes );

	if( pkt != 0 ) {

		free( pkt );
	}

	return;
}
/* }}} */

/* {{{ proto int orbput( int orbfd, string srcname, double time, string packet, int nbytes ) */
PHP_FUNCTION(orbput)
{
	long	orbfd;
	char	*srcname;
	int	*srcname_len;
	double	time;
	char	*packet;
	int	*packet_len;
	long	nbytes;
	int	rc;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 5 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "lsdsl", 
		&orbfd, &srcname, &srcname_len, &time,
		&packet, &packet_len, &nbytes )
	    == FAILURE) {

		return;
	}
	
	rc = orbput( (int) orbfd, srcname, time, packet, (int) nbytes );

	RETURN_LONG( rc );
}
/* }}} */

/* {{{ proto int orbputx( int orbfd, string srcname, double time, string packet, int nbytes ) */
PHP_FUNCTION(orbputx)
{
	long	orbfd;
	char	*srcname;
	int	*srcname_len;
	double	time;
	char	*packet;
	int	*packet_len;
	long	nbytes;
	int	pktid;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 5 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "lsdsl", 
		&orbfd, &srcname, &srcname_len, &time,
		&packet, &packet_len, &nbytes )
	    == FAILURE) {

		return;
	}
	
	pktid = orbputx( (int) orbfd, srcname, time, packet, (int) nbytes );

	RETURN_LONG( pktid );
}
/* }}} */

/* {{{ proto int orbtell( int orbfd ) */
PHP_FUNCTION(orbtell)
{
	long	orbfd;
	int	pktid;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "l", &orbfd )
	    == FAILURE) {

		return;
	}
	
	pktid = orbtell( (int) orbfd );

	RETURN_LONG( pktid );
}
/* }}} */

/* {{{ proto int orbping( int orbfd ) */
PHP_FUNCTION(orbping)
{
	long	orbfd;
	int	version;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "l", &orbfd )
	    == FAILURE) {

		return;
	}
	
	orbping( (int) orbfd, &version );

	RETURN_LONG( version );
}
/* }}} */

/* {{{ proto int orbselect( int orbfd, string regex ) */
PHP_FUNCTION(orbselect)
{
	long	orbfd;
	char	*regex;
	int	regex_len;
	int	argc = ZEND_NUM_ARGS();
	int	rc;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ls",
				 &orbfd, &regex, &regex_len )
	    == FAILURE) {

		return;
	}
	
	rc = orbselect( (int) orbfd, regex );

	RETURN_LONG( rc );
}
/* }}} */

/* {{{ proto int orbreject( int orbfd, string regex ) */
PHP_FUNCTION(orbreject)
{
	long	orbfd;
	char	*regex;
	int	regex_len;
	int	argc = ZEND_NUM_ARGS();
	int	rc;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ls",
				 &orbfd, &regex, &regex_len )
	    == FAILURE) {

		return;
	}
	
	rc = orbreject( (int) orbfd, regex );

	RETURN_LONG( rc );
}
/* }}} */

/* {{{ proto int orbafter( int orbfd, double time ) */
PHP_FUNCTION(orbafter)
{
	long	orbfd;
	double	time;
	int	argc = ZEND_NUM_ARGS();
	int	pktid;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ld",
				 &orbfd, &time )
	    == FAILURE) {

		return;
	}
	
	pktid = orbafter( (int) orbfd, time );

	RETURN_LONG( pktid );
}
/* }}} */

/* {{{ proto int orbposition( int orbfd, string from ) */
PHP_FUNCTION(orbposition)
{
	long	orbfd;
	char	*from;
	int	from_len;
	int	argc = ZEND_NUM_ARGS();
	int	pktid;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ls",
				 &orbfd, &from, &from_len )
	    == FAILURE) {

		return;
	}
	
	pktid = orbposition( (int) orbfd, from );

	RETURN_LONG( pktid );
}
/* }}} */

/* {{{ proto int orbseek( int orbfd, mixed which ) */
PHP_FUNCTION(orbseek)
{
	long	orbfd;
	zval	*zval_which;
	int	which;
	int	argc = ZEND_NUM_ARGS();
	int	pktid;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "lz",
				 &orbfd, &zval_which )
	    == FAILURE) {

		return;
	}

	if( Z_TYPE_P( zval_which ) == IS_STRING ) {

		which = xlatname( Z_STRVAL_P( zval_which ), Orbconst, Orbconstn );

		if( which == -1 ) {

			zend_error( E_ERROR, 
		     	   "orbseek 'which' code not recognized" );

			return;
		}
	
	} else if( Z_TYPE_P( zval_which ) == IS_LONG ) {

		which = Z_LVAL_P( zval_which );

	} else {

		zend_error( E_ERROR, 
		     "orbseek 'which' argument must be string or integer" );

		return;
	}

	pktid = orbseek( (int) orbfd, which );

	RETURN_LONG( pktid );
}
/* }}} */

/* {{{ proto int orbwait( int orbfd, string re, double mintime, double timeout ) */
PHP_FUNCTION(orbwait)
{
	long	orbfd;
	char	*re;
	int	re_len;
	double	mintime;
	double	timeout;
	int	argc = ZEND_NUM_ARGS();
	int	rc;

	if( argc != 4 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "lsdd",
				 &orbfd, &re, &re_len,
				 &mintime, &timeout)
	    == FAILURE) {

		return;
	}
	
	rc = orbwait( (int) orbfd, re, mintime, timeout );

	RETURN_LONG( rc );
}
/* }}} */

/* {{{ proto int pforbstat( int orbfd, int flags ) */
PHP_FUNCTION(pforbstat)
{
	long	orbfd;
	long 	flags;
	int	argc = ZEND_NUM_ARGS();
	Pf 	*pf;

	if( argc != 2 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "ll", &orbfd, &flags )
	    == FAILURE) {

		return;
	}
	
	pf = pforbstat( (int) orbfd, (int) flags );

	if( pf != NULL ) {

		pf2zval( pf, return_value );
	
		pffree( pf );
	}

	return;
}
/* }}} */

/* {{{ proto int unstuffPkt( string srcname, double time, string packet, int nbytes ) */
PHP_FUNCTION(unstuffPkt)
{
	Packet	*pkt = 0;
	char	*srcname;
	int	*srcname_len;
	double	time;
	char	*packet;
	int	*packet_len;
	long	nbytes;
	int	rc;
	zval	*zval_pkt;
	int	argc = ZEND_NUM_ARGS();

	if( argc != 4 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "sdsl", 
		&srcname, &srcname_len, &time,
		&packet, &packet_len, &nbytes )
	    == FAILURE) {

		return;
	}
	
	rc = unstuffPkt( srcname, time, packet, (int) nbytes, &pkt );

	if( rc < 0 ) {
		
		zend_error( E_ERROR, "unstuffPkt failed" );

		return;
	}

	zval_pkt = pkt2zval( rc, pkt );

	array_init( return_value );

	add_next_index_long( return_value, rc );
	add_next_index_zval( return_value, zval_pkt );

	return;
}
/* }}} */

PHP_METHOD(orb_pkt, PacketType)
{
	zval	*this;
	php_orb_pkt_obj *intern;

	this = getThis();

	intern = (php_orb_pkt_obj *) 
		    zend_objects_get_address( this TSRMLS_CC );

	array_init( return_value );

	add_next_index_long( return_value, intern->type );
	add_next_index_string( return_value, intern->pkt->pkttype->desc, 1 );

	return;
}

/* {{{ proto array split_srcname( string srcname ) */
PHP_FUNCTION(split_srcname)
{
	char	*srcname;
	int	srcname_len;
	int	argc = ZEND_NUM_ARGS();
	Srcname parts;

	if( argc != 1 ) {

		WRONG_PARAM_COUNT;
	}

	if( zend_parse_parameters( argc TSRMLS_CC, "s", 
					&srcname, &srcname_len )
	    == FAILURE) {

		return;
	}

	split_srcname( srcname, &parts );

	array_init( return_value );

	add_assoc_string_ex( return_value, 
			     "net", strlen( "net" ) + 1, 
			     parts.src_net, 1 );

	add_assoc_string_ex( return_value, 
			     "sta", strlen( "sta" ) + 1, 
			     parts.src_sta, 1 );

	add_assoc_string_ex( return_value, 
			     "chan", strlen( "chan" ) + 1, 
			     parts.src_chan, 1 );

	add_assoc_string_ex( return_value, 
			     "loc", strlen( "loc" ) + 1, 
			     parts.src_loc, 1 );

	add_assoc_string_ex( return_value, 
			     "suffix", strlen( "suffix" ) + 1, 
			     parts.src_suffix, 1 );

	add_assoc_string_ex( return_value, 
			     "subcode", strlen( "subcode" ) + 1, 
			     parts.src_subcode, 1 );

	return;
}
/* }}} */


/* local variables
 * End:
 * vim600: fdm=marker
 */
