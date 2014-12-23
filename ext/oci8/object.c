/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2002-2013 Kubo Takehiro <kubo@jiubao.org>
 */

/*
 *
 * Document-class: OCI8::TDO
 *
 * OCI8::TDO is the class for Type Descriptor Object, which describe
 * Oracle's object type.
 *
 * An instance of OCI8::TDO is specific to the connection. This means
 * One TDO instance for a connection is not available to another connection.
 */
#include "oci8.h"
#include <orid.h>

static VALUE cOCI8TDO;
static VALUE cOCI8NamedType;
static VALUE cOCI8NamedCollection;
static VALUE cOCI8BindNamedType;
static ID id_to_value;
static ID id_set_attributes;

#define TO_TDO(obj) oci8_get_handle((obj), cOCI8TDO)

typedef struct {
    oci8_base_t base;
    VALUE tdo;
    char **instancep;
    char **null_structp;
} oci8_named_type_t;

enum {
    ATTR_INVALID = 0,
    ATTR_STRING,
    ATTR_RAW,
    ATTR_OCINUMBER,
    ATTR_FLOAT,
    ATTR_INTEGER,
    ATTR_OCIDATE,
    ATTR_BINARY_DOUBLE,
    ATTR_BINARY_FLOAT,
    ATTR_NAMED_TYPE,
    ATTR_NAMED_COLLECTION,
    ATTR_CLOB,
    ATTR_NCLOB,
    ATTR_BLOB,
    ATTR_BFILE,
};

static VALUE get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind);
static void set_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind, VALUE val);

static void oci8_tdo_mark(oci8_base_t *base)
{
    if (base->parent != NULL) {
        rb_gc_mark(base->parent->self);
    }
}

static void oci8_tdo_free(oci8_base_t *base)
{
    if (base->hp.tdo != NULL) {
        OCIObjectUnpin(oci8_envhp, oci8_errhp, base->hp.tdo);
        base->hp.tdo = NULL;
    }
}

static VALUE oci8_tdo_setup(VALUE self, VALUE svc, VALUE md_obj)
{
    oci8_base_t *tdo = TO_TDO(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(svc);
    oci8_base_t *md;
    OCIRef *tdo_ref = NULL;

    Check_Object(md_obj, cOCI8MetadataBase);
    md = DATA_PTR(md_obj);

    if (tdo->hp.tdo != NULL) {
        OCIObjectUnpin(oci8_envhp, oci8_errhp, tdo->hp.tdo);
        tdo->hp.tdo = NULL;
    }
    chker2(OCIAttrGet(md->hp.ptr, OCI_DTYPE_PARAM, &tdo_ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp),
           &svcctx->base);
    if (tdo_ref == NULL)
        return Qnil;
    chker2(OCIObjectPin_nb(svcctx, oci8_envhp, oci8_errhp, tdo_ref, 0, OCI_PIN_ANY, OCI_DURATION_SESSION, OCI_LOCK_NONE, &tdo->hp.ptr),
           &svcctx->base);
    oci8_link_to_parent(tdo, &svcctx->base);
    return self;
}

static oci8_base_vtable_t oci8_tdo_vtable = {
    oci8_tdo_mark,
    oci8_tdo_free,
    sizeof(oci8_base_t)
};

static void oci8_named_type_mark(oci8_base_t *base)
{
    if (base->parent != NULL) {
        rb_gc_mark(base->parent->self);
    }
}

static void oci8_named_type_free(oci8_base_t *base)
{
    oci8_named_type_t *obj = (oci8_named_type_t *)base;
    obj->instancep = NULL;
    obj->null_structp = NULL;
}

static VALUE oci8_named_type_initialize(VALUE self)
{
    oci8_named_type_t *obj = DATA_PTR(self);

    obj->tdo = Qnil;
    obj->instancep = NULL;
    obj->null_structp = NULL;
    return Qnil;
}

static VALUE oci8_named_type_tdo(VALUE self)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    return obj->tdo;
}

static void oci8_named_type_check_offset(VALUE self, VALUE val_offset, VALUE ind_offset, size_t val_size, void **instancep, OCIInd **indp)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    if (obj->instancep == NULL || obj->null_structp == NULL) {
        rb_raise(rb_eRuntimeError, "%s is not initialized or freed", rb_obj_classname(self));
    }
    Check_Type(val_offset, T_FIXNUM);
    Check_Type(ind_offset, T_FIXNUM);
    *instancep = (void*)(*obj->instancep + FIX2INT(val_offset));
    *indp = (OCIInd *)(*obj->null_structp + FIX2INT(ind_offset));
}

/*
 * attribute
 */
static VALUE oci8_named_type_get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, VALUE val_offset, VALUE ind_offset)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &data, &ind);
    return get_attribute(self, datatype, typeinfo, data, ind);
}

static VALUE get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind)
{
    VALUE rv;
    VALUE tmp_obj;
    oci8_named_type_t *obj;

    if (*ind) {
        return Qnil;
    }
    Check_Type(datatype, T_FIXNUM);
    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
        return rb_external_str_new_with_enc(TO_CHARPTR(OCIStringPtr(oci8_envhp, *(OCIString **)data)),
                                            OCIStringSize(oci8_envhp, *(OCIString **)data),
                                            oci8_encoding);
    case ATTR_RAW:
        return rb_str_new(TO_CHARPTR(OCIRawPtr(oci8_envhp, *(OCIRaw **)data)),
                          OCIRawSize(oci8_envhp, *(OCIRaw **)data));
    case ATTR_OCINUMBER:
        return oci8_make_ocinumber((OCINumber *)data, oci8_errhp);
    case ATTR_FLOAT:
        return oci8_make_float((OCINumber *)data, oci8_errhp);
    case ATTR_INTEGER:
        return oci8_make_integer((OCINumber *)data, oci8_errhp);
    case ATTR_OCIDATE:
        return oci8_make_ocidate((OCIDate *)data);
    case ATTR_BINARY_DOUBLE:
        return rb_float_new(*(double*)data);
    case ATTR_BINARY_FLOAT:
        return rb_float_new((double)*(float*)data);
    case ATTR_NAMED_TYPE:
        Check_Object(typeinfo, cOCI8TDO);
        /* Be carefull. Don't use *tmp_obj* out of this function. */
        tmp_obj = rb_class_new_instance(0, NULL, cOCI8NamedType);
        obj = DATA_PTR(tmp_obj);
        RB_OBJ_WRITE(tmp_obj, &obj->tdo, typeinfo);
        obj->instancep = (char**)&data;
        obj->null_structp = (char**)&ind;
        oci8_link_to_parent(&obj->base, DATA_PTR(self));
        rv = rb_funcall(tmp_obj, id_to_value, 0);
        oci8_unlink_from_parent(&obj->base);
        return rv;
    case ATTR_NAMED_COLLECTION:
        Check_Object(typeinfo, cOCI8TDO);
        /* Be carefull. Don't use *tmp_obj* out of this function. */
        tmp_obj = rb_class_new_instance(0, NULL, cOCI8NamedCollection);
        obj = DATA_PTR(tmp_obj);
        RB_OBJ_WRITE(tmp_obj, &obj->tdo, typeinfo);
        obj->instancep = (char**)data;
        obj->null_structp = (char**)&ind;
        oci8_link_to_parent(&obj->base, DATA_PTR(self));
        rv = rb_funcall(tmp_obj, id_to_value, 0);
        oci8_unlink_from_parent(&obj->base);
        return rv;
    case ATTR_CLOB:
        return oci8_make_clob(oci8_get_svcctx(typeinfo), *(OCILobLocator**)data);
    case ATTR_NCLOB:
        return oci8_make_nclob(oci8_get_svcctx(typeinfo), *(OCILobLocator**)data);
    case ATTR_BLOB:
        return oci8_make_blob(oci8_get_svcctx(typeinfo), *(OCILobLocator**)data);
    case ATTR_BFILE:
        return oci8_make_bfile(oci8_get_svcctx(typeinfo), *(OCILobLocator**)data);
    default:
        rb_raise(rb_eRuntimeError, "not supported datatype");
    }
}

static VALUE oci8_named_coll_get_coll_element(VALUE self, VALUE datatype, VALUE typeinfo)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    OCIColl *coll;
    void *data;
    OCIInd *ind;
    VALUE ary;
    sb4 size;
    sb4 idx;

    if (obj->instancep == NULL || obj->null_structp == NULL) {
        rb_raise(rb_eRuntimeError, "%s is not initialized or freed", rb_obj_classname(self));
    }
    coll = (OCIColl*)*obj->instancep;
    ind = (OCIInd*)*obj->null_structp;
    if (*ind) {
        return Qnil;
    }
    chker2(OCICollSize(oci8_envhp, oci8_errhp, coll, &size), &obj->base);
    ary = rb_ary_new2(size);
    for (idx = 0; idx < size; idx++) {
        boolean exists;
        chker2(OCICollGetElem(oci8_envhp, oci8_errhp, coll, idx, &exists, &data, (dvoid**)&ind), &obj->base);
        if (exists) {
            void *tmp;
            if (datatype == INT2FIX(ATTR_NAMED_COLLECTION)) {
                tmp = data;
                data = &tmp;
            }
            rb_ary_store(ary, idx, get_attribute(self, datatype, typeinfo, data, ind));
        }
    }
    return ary;
}

static VALUE oci8_named_type_set_attribute(VALUE self, VALUE datatype, VALUE typeinfo, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &data, &ind);
    set_attribute(self, datatype, typeinfo, data, ind, val);
    return Qnil;
}

static VALUE oci8_named_type_null_p(VALUE self)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, INT2FIX(0), INT2FIX(0), sizeof(void*), &data, &ind);
    return *ind ? Qtrue : Qfalse;
}

static VALUE oci8_named_type_set_null(VALUE self, VALUE val)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, INT2FIX(0), INT2FIX(0), sizeof(void*), &data, &ind);
    *ind = RTEST(val) ? -1 : 0;
    return val;
}

typedef struct {
    VALUE self;
    VALUE datatype;
    VALUE typeinfo;
    VALUE val;
    OCIColl *coll;
    union {
        void *ptr;
        OCINumber num;
        double dbl;
        float flt;
    } data;
    OCIInd ind; /* for data.num, data.dbl, data.flt */
    OCIInd *indp;
} set_coll_element_cb_data_t;

static VALUE set_coll_element_func(set_coll_element_cb_data_t *cb_data);
static VALUE set_coll_element_ensure(set_coll_element_cb_data_t *cb_data);

static VALUE oci8_named_coll_set_coll_element(VALUE self, VALUE datatype, VALUE typeinfo, VALUE val)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    OCIInd *ind;
    set_coll_element_cb_data_t cb_data;
    oci8_base_t *tdo;
    oci8_base_t *svcctx;

    if (obj->instancep == NULL || obj->null_structp == NULL) {
        rb_raise(rb_eRuntimeError, "%s is not initialized or freed", rb_obj_classname(self));
    }
    Check_Type(datatype, T_FIXNUM);
    ind = (OCIInd*)*obj->null_structp;
    if (NIL_P(val)) {
        *ind = -1;
        return Qnil;
    }
    Check_Type(val, T_ARRAY);
    /* search svcctx */
    svcctx = DATA_PTR(obj->tdo);
    while (svcctx->type != OCI_HTYPE_SVCCTX) {
        svcctx = svcctx->parent;
    }
    /* setup cb_data */
    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.self = self;
    cb_data.datatype = datatype;
    cb_data.typeinfo = typeinfo;
    cb_data.val = val;
    cb_data.coll = (OCIColl*)*obj->instancep;
    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
        chker2(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->hp.svc, OCI_TYPECODE_VARCHAR2, NULL, NULL, OCI_DURATION_SESSION, TRUE, &cb_data.data.ptr),
               svcctx);
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_RAW:
        chker2(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->hp.svc, OCI_TYPECODE_RAW, NULL, NULL, OCI_DURATION_SESSION, TRUE, &cb_data.data.ptr),
               svcctx);
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_OCINUMBER:
    case ATTR_FLOAT:
    case ATTR_INTEGER:
        OCINumberSetZero(oci8_errhp, &cb_data.data.num);
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_OCIDATE:
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_BINARY_DOUBLE:
        cb_data.data.dbl = 0.0;
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_BINARY_FLOAT:
        cb_data.data.flt = 0.0;
        cb_data.indp = &cb_data.ind;
        break;
    case ATTR_NAMED_TYPE:
        Check_Object(typeinfo, cOCI8TDO);
        tdo = DATA_PTR(typeinfo);
        chker2(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->hp.svc, OCI_TYPECODE_OBJECT, tdo->hp.tdo, NULL, OCI_DURATION_SESSION, TRUE, &cb_data.data.ptr),
               svcctx);
        chker2(OCIObjectGetInd(oci8_envhp, oci8_errhp, cb_data.data.ptr, (dvoid**)&cb_data.indp),
               svcctx);
        break;
    case ATTR_NAMED_COLLECTION:
        Check_Object(typeinfo, cOCI8TDO);
        tdo = DATA_PTR(typeinfo);
        chker2(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->hp.svc, OCI_TYPECODE_NAMEDCOLLECTION, tdo->hp.tdo, NULL, OCI_DURATION_SESSION, TRUE, &cb_data.data.ptr),
               svcctx);
        chker2(OCIObjectGetInd(oci8_envhp, oci8_errhp, cb_data.data.ptr, (dvoid**)&cb_data.indp),
               svcctx);
        break;
    case ATTR_CLOB:
    case ATTR_NCLOB:
    case ATTR_BLOB:
    case ATTR_BFILE:
        rb_raise(rb_eRuntimeError, "Could not set LOB objects to collection types yet.");
    default:
        rb_raise(rb_eRuntimeError, "not supported datatype");
    }
#if 0
    /* TODO: */
    rb_ensure(set_coll_element_func, (VALUE)&cb_data, set_coll_element_ensure, (VALUE)&cb_data);
#else
    set_coll_element_func(&cb_data);
    set_coll_element_ensure(&cb_data);
#endif
    *ind = 0;
    return Qnil;
}

static VALUE set_coll_element_func(set_coll_element_cb_data_t *cb_data)
{
    VALUE self = cb_data->self;
    VALUE val = cb_data->val;
    VALUE datatype = cb_data->datatype;
    VALUE typeinfo = cb_data->typeinfo;
    OCIColl *coll = cb_data->coll;
    sb4 size;
    sb4 idx;
    void *elem_ptr;

    chkerr(OCICollSize(oci8_envhp, oci8_errhp, coll, &size));
    if (RARRAY_LEN(val) < size) {
        chkerr(OCICollTrim(oci8_envhp, oci8_errhp, size - RARRAY_LEN(val), coll));
    }
    for (idx = 0; idx < RARRAY_LEN(val); idx++) {
        switch (FIX2INT(datatype)) {
        case ATTR_NAMED_TYPE:
            set_attribute(self, datatype, typeinfo, cb_data->data.ptr, cb_data->indp, RARRAY_AREF(val, idx));
            break;
        default:
            set_attribute(self, datatype, typeinfo, (void*)&cb_data->data, cb_data->indp, RARRAY_AREF(val, idx));
            break;
        }
        switch (FIX2INT(datatype)) {
        case ATTR_OCINUMBER:
        case ATTR_FLOAT:
        case ATTR_INTEGER:
        case ATTR_OCIDATE:
        case ATTR_BINARY_DOUBLE:
        case ATTR_BINARY_FLOAT:
            elem_ptr = &cb_data->data;
            break;
        default:
            elem_ptr = cb_data->data.ptr;
            break;
        }
        if (idx < size) {
            chkerr(OCICollAssignElem(oci8_envhp, oci8_errhp, idx, elem_ptr, cb_data->indp, cb_data->coll));
        } else {
            chkerr(OCICollAppend(oci8_envhp, oci8_errhp, elem_ptr, cb_data->indp, coll));
        }
    }
    return Qnil;
}

static VALUE set_coll_element_ensure(set_coll_element_cb_data_t *cb_data)
{
    VALUE datatype = cb_data->datatype;

    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
    case ATTR_RAW:
    case ATTR_NAMED_TYPE:
    case ATTR_NAMED_COLLECTION:
        if (cb_data->data.ptr != NULL) {
            OCIObjectFree(oci8_envhp, oci8_errhp, cb_data->data.ptr, OCI_DEFAULT);
        }
        break;
    case ATTR_OCINUMBER:
    case ATTR_FLOAT:
    case ATTR_INTEGER:
    case ATTR_OCIDATE:
    case ATTR_BINARY_DOUBLE:
    case ATTR_BINARY_FLOAT:
        break;
    }
    return Qnil;
}


static void set_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind, VALUE val)
{
    VALUE tmp_obj;
    oci8_named_type_t *obj;

    if (NIL_P(val)) {
        *ind = -1;
        return;
    }
    Check_Type(datatype, T_FIXNUM);
    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
        OCI8StringValue(val);
        chkerr(OCIStringAssignText(oci8_envhp, oci8_errhp,
                                   RSTRING_ORATEXT(val), RSTRING_LEN(val),
                                   (OCIString **)data));
        break;
    case ATTR_RAW:
        StringValue(val);
        chkerr(OCIRawAssignBytes(oci8_envhp, oci8_errhp,
                                 RSTRING_ORATEXT(val), RSTRING_LEN(val),
                                 (OCIRaw **)data));
        break;
    case ATTR_OCINUMBER:
    case ATTR_FLOAT:
        oci8_set_ocinumber((OCINumber*)data, val, oci8_errhp);
        break;
    case ATTR_INTEGER:
        oci8_set_integer((OCINumber*)data, val, oci8_errhp);
        break;
    case ATTR_OCIDATE:
        oci8_set_ocidate((OCIDate*)data, val);
        break;
    case ATTR_BINARY_DOUBLE:
        *(double*)data = NUM2DBL(val);
        break;
    case ATTR_BINARY_FLOAT:
        *(float*)data = (float)NUM2DBL(val);
        break;
    case ATTR_NAMED_TYPE:
        Check_Object(typeinfo, cOCI8TDO);
        /* Be carefull. Don't use *tmp_obj* out of this function. */
        tmp_obj = rb_class_new_instance(0, NULL, cOCI8NamedType);
        obj = DATA_PTR(tmp_obj);
        RB_OBJ_WRITE(tmp_obj, &obj->tdo, typeinfo);
        obj->instancep = (char**)&data;
        obj->null_structp = (char**)&ind;
        oci8_link_to_parent(&obj->base, DATA_PTR(self));
        rb_funcall(tmp_obj, id_set_attributes, 1, val);
        oci8_unlink_from_parent(&obj->base);
        break;
    case ATTR_NAMED_COLLECTION:
        Check_Object(typeinfo, cOCI8TDO);
        /* Be carefull. Don't use *tmp_obj* out of this function. */
        tmp_obj = rb_class_new_instance(0, NULL, cOCI8NamedCollection);
        obj = DATA_PTR(tmp_obj);
        RB_OBJ_WRITE(tmp_obj, &obj->tdo, typeinfo);
        obj->instancep = (char**)data;
        obj->null_structp = (char**)&ind;
        oci8_link_to_parent(&obj->base, DATA_PTR(self));
        rb_funcall(tmp_obj, id_set_attributes, 1, val);
        oci8_unlink_from_parent(&obj->base);
        break;
    case ATTR_CLOB:
        oci8_assign_clob(oci8_get_svcctx(typeinfo), val, (OCILobLocator **)data);
        break;
    case ATTR_NCLOB:
        oci8_assign_nclob(oci8_get_svcctx(typeinfo), val, (OCILobLocator **)data);
        break;
    case ATTR_BLOB:
        oci8_assign_blob(oci8_get_svcctx(typeinfo), val, (OCILobLocator **)data);
        break;
    case ATTR_BFILE:
        oci8_assign_bfile(oci8_get_svcctx(typeinfo), val, (OCILobLocator **)data);
        break;
    default:
        rb_raise(rb_eRuntimeError, "not supported datatype");
    }
    *ind = 0;
}

static oci8_base_vtable_t oci8_named_type_vtable = {
    oci8_named_type_mark,
    oci8_named_type_free,
    sizeof(oci8_named_type_t)
};

static void bind_named_type_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;

    if (oho != NULL) {
        ub4 idx = 0;

        do {
            rb_gc_mark(oho[idx].obj);
        } while (++idx < obind->maxar_sz);
    }
    rb_gc_mark(obind->tdo);
}

static void bind_named_type_free(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;

    if (oho != NULL) {
        ub4 idx = 0;

        do {
            if (oho[idx].hp != NULL) {
                OCIObjectFree(oci8_envhp, oci8_errhp, oho[idx].hp, OCI_DEFAULT);
                oho[idx].hp = NULL;
            }
        } while (++idx < obind->maxar_sz);
    }
    oci8_bind_free(base);
}

static VALUE bind_named_type_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    return oho->obj;
}

static void bind_named_type_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    rb_raise(rb_eRuntimeError, "not supported");
}

static void bind_named_type_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    VALUE tdo_obj = length;

    obind->value_sz = sizeof(void*);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);

    Check_Object(tdo_obj, cOCI8TDO);
    RB_OBJ_WRITE(obind->base.self, &obind->tdo, tdo_obj);
}

static void bind_named_type_init_elem(oci8_bind_t *obind, VALUE svc)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_base_t *tdo = DATA_PTR(obind->tdo);
    OCITypeCode tc = OCITypeTypeCode(oci8_envhp, oci8_errhp, tdo->hp.tdo);
    VALUE klass = Qnil;
    oci8_named_type_t *obj;
    oci8_svcctx_t *svcctx;
    ub4 idx = 0;

    switch (tc) {
    case OCI_TYPECODE_OBJECT:
        klass = cOCI8NamedType;
        break;
    case OCI_TYPECODE_NAMEDCOLLECTION:
        klass = cOCI8NamedCollection;
        break;
    }
    svcctx = oci8_get_svcctx(svc);
    do {
        oho[idx].obj = rb_class_new_instance(0, NULL, klass);
        obj = DATA_PTR(oho[idx].obj);
        RB_OBJ_WRITE(oho[idx].obj, &obj->tdo, obind->tdo);
        obj->instancep = (char**)&oho[idx].hp;
        obj->null_structp = (char**)&obind->u.null_structs[idx];
        oci8_link_to_parent(&obj->base, &obind->base);

        chker2(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->base.hp.svc, tc, tdo->hp.tdo, NULL, OCI_DURATION_SESSION, TRUE, (dvoid**)obj->instancep),
               &svcctx->base);
        chker2(OCIObjectGetInd(oci8_envhp, oci8_errhp, (dvoid*)*obj->instancep, (dvoid**)obj->null_structp),
               &svcctx->base);
        *(OCIInd*)*obj->null_structp = -1;
    } while (++idx < obind->maxar_sz);
}

static void bind_name_type_post_bind_hook(oci8_bind_t *obind)
{
    oci8_base_t *tdo = DATA_PTR(obind->tdo);
    switch (obind->base.type) {
    case OCI_HTYPE_DEFINE:
        chker2(OCIDefineObject(obind->base.hp.dfn, oci8_errhp, tdo->hp.tdo,
                               obind->valuep, 0, obind->u.null_structs, 0),
               &obind->base);
        break;
    case OCI_HTYPE_BIND:
        chker2(OCIBindObject(obind->base.hp.bnd, oci8_errhp, tdo->hp.tdo,
                             obind->valuep, 0, obind->u.null_structs, 0),
               &obind->base);
        break;
    }
}

static const oci8_bind_vtable_t bind_named_type_vtable = {
    {
        bind_named_type_mark,
        bind_named_type_free,
        sizeof(oci8_bind_t)
    },
    bind_named_type_get,
    bind_named_type_set,
    bind_named_type_init,
    bind_named_type_init_elem,
    NULL,
    SQLT_NTY,
    bind_name_type_post_bind_hook,
};

void Init_oci_object(VALUE cOCI8)
{
    id_to_value = rb_intern("to_value");
    id_set_attributes = rb_intern("attributes=");

    /* OCI8::TDO */
    cOCI8TDO = oci8_define_class_under(cOCI8, "TDO", &oci8_tdo_vtable);
    rb_define_private_method(cOCI8TDO, "setup", oci8_tdo_setup, 2);
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_STRING", INT2FIX(ATTR_STRING));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_RAW", INT2FIX(ATTR_RAW));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_OCINUMBER", INT2FIX(ATTR_OCINUMBER));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_FLOAT", INT2FIX(ATTR_FLOAT));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_INTEGER", INT2FIX(ATTR_INTEGER));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_OCIDATE", INT2FIX(ATTR_OCIDATE));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_BINARY_DOUBLE", INT2FIX(ATTR_BINARY_DOUBLE));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_BINARY_FLOAT", INT2FIX(ATTR_BINARY_FLOAT));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_NAMED_TYPE", INT2FIX(ATTR_NAMED_TYPE));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_NAMED_COLLECTION", INT2FIX(ATTR_NAMED_COLLECTION));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_CLOB", INT2FIX(ATTR_CLOB));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_NCLOB", INT2FIX(ATTR_NCLOB));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_BLOB", INT2FIX(ATTR_BLOB));
    /* @private */
    rb_define_const(cOCI8TDO, "ATTR_BFILE", INT2FIX(ATTR_BFILE));
#define ALIGNMENT_OF(type) (size_t)&(((struct {char c; type t;}*)0)->t)
    /* @private */
    rb_define_const(cOCI8TDO, "SIZE_OF_POINTER", INT2FIX(sizeof(void *)));
    /* @private */
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_POINTER", INT2FIX(ALIGNMENT_OF(void *)));
    /* @private */
    rb_define_const(cOCI8TDO, "SIZE_OF_OCINUMBER", INT2FIX(sizeof(OCINumber)));
    /* @private */
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_OCINUMBER", INT2FIX(ALIGNMENT_OF(OCINumber)));
    /* @private */
    rb_define_const(cOCI8TDO, "SIZE_OF_OCIDATE", INT2FIX(sizeof(OCIDate)));
    /* @private */
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_OCIDATE", INT2FIX(ALIGNMENT_OF(OCIDate)));
    /* @private */
    rb_define_const(cOCI8TDO, "SIZE_OF_FLOAT", INT2FIX(sizeof(float)));
    /* @private */
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_FLOAT", INT2FIX(ALIGNMENT_OF(float)));
    /* @private */
    rb_define_const(cOCI8TDO, "SIZE_OF_DOUBLE", INT2FIX(sizeof(double)));
    /* @private */
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_DOUBLE", INT2FIX(ALIGNMENT_OF(double)));

    /* OCI8::NamedType */
    cOCI8NamedType = oci8_define_class_under(cOCI8, "NamedType", &oci8_named_type_vtable);
    rb_define_method(cOCI8NamedType, "initialize", oci8_named_type_initialize, 0);
    rb_define_method(cOCI8NamedType, "tdo", oci8_named_type_tdo, 0);
    rb_define_private_method(cOCI8NamedType, "get_attribute", oci8_named_type_get_attribute, 4);
    rb_define_private_method(cOCI8NamedType, "set_attribute", oci8_named_type_set_attribute, 5);
    rb_define_method(cOCI8NamedType, "null?", oci8_named_type_null_p, 0);
    rb_define_method(cOCI8NamedType, "null=", oci8_named_type_set_null, 1);

    /* OCI8::NamedCollection */
    cOCI8NamedCollection = oci8_define_class_under(cOCI8, "NamedCollection", &oci8_named_type_vtable);
    rb_define_method(cOCI8NamedCollection, "initialize", oci8_named_type_initialize, 0);
    rb_define_method(cOCI8NamedCollection, "tdo", oci8_named_type_tdo, 0);
    rb_define_private_method(cOCI8NamedCollection, "get_coll_element", oci8_named_coll_get_coll_element, 2);
    rb_define_private_method(cOCI8NamedCollection, "set_coll_element", oci8_named_coll_set_coll_element, 3);

    /* OCI8::BindType::NamedType */
    cOCI8BindNamedType = oci8_define_bind_class("NamedType", &bind_named_type_vtable);
}
