#include "d_mod_scalar.h"

/**
 * Scalar Module
 */

drax_value __d_scalar_at(d_vm* vm, int* stat) {
  #define RETURN_AT_TO_TYPE(_tp, _dv, _ll, _n)\
    _tp* _dv = (_tp*) _ll->elems;\
    return num_to_draxvalue((_tp) _dv[(int) _n]);

  drax_value b = pop(vm);
  drax_value a = pop(vm);

  return_if_is_not_scalar(a, stat);
  return_if_is_not_number(b, stat);

  double n = CAST_NUMBER(b);
  drax_scalar* ll = CAST_SCALAR(a);

  if (n < 0) { n = ll->length + n; }

  if (n < 0 || n >= ll->length) {
    DX_SUCESS_FN(stat);
    return DRAX_NIL_VAL;
  }

  DX_SUCESS_FN(stat);

  if (DIT_f32 == ll->_stype) {
    RETURN_AT_TO_TYPE(float, _f32, ll, n);
  }

  if (DIT_f64 == ll->_stype) {
    RETURN_AT_TO_TYPE(double, _f64, ll, n);
  }

  return ll->elems[(int) n];
}

drax_value __d_scalar_concat(d_vm* vm, int* stat) {
  #define DO_MEMCPY_TO_TYPE(_tp, _l1, _l2, _l3)\
    _tp* _dv3 = (_tp*) _l3->elems;\
    memcpy(_dv3, (_tp*) _l1->elems, _l1->length * sizeof(_tp));\
    memcpy(_dv3 + _l1->length, (_tp*) _l2->elems, _l2->length * sizeof(_tp));

  drax_value b = pop(vm);
  drax_value a = pop(vm);

  return_if_is_not_scalar(a, stat);
  return_if_is_not_scalar(b, stat);

  drax_scalar* l1 = CAST_SCALAR(a);
  drax_scalar* l2 = CAST_SCALAR(b);
  
  if (l1->_stype != l2->_stype) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Scalar concat with different types."));
  }

  drax_scalar* l = new_dscalar(vm, l1->length + l2->length, l1->_stype);
  l->length = l1->length + l2->length;
  l->cap = l->length;

  if (DIT_f32 == l1->_stype) {
    DO_MEMCPY_TO_TYPE(float, l1, l2, l);
  } else if (DIT_f64 == l1->_stype) {
    DO_MEMCPY_TO_TYPE(double, l1, l2, l);
  } else {
    memcpy(l->elems, l1->elems, l1->length * sizeof(drax_value));
    memcpy(l->elems + l1->length, l2->elems, l2->length * sizeof(drax_value));
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(l);
}

drax_value __d_scalar_head(d_vm* vm, int* stat) {
  #define RETURN_VAL_TO_TYPE(_tp, _l1)\
    _tp* _dv = (_tp*) _l1->elems;\
    return _l1->length > 0 ? num_to_draxvalue(_dv[0]) : DRAX_NIL_VAL;

  drax_value a = pop(vm);

  return_if_is_not_scalar(a, stat);
  drax_scalar* l = CAST_SCALAR(a);

  DX_SUCESS_FN(stat);

  if (DIT_f32 == l->_stype) {
    RETURN_VAL_TO_TYPE(float, l);
  }

  if (DIT_f64 == l->_stype) {
    RETURN_VAL_TO_TYPE(double, l);
  }

  return l->length > 0 ? l->elems[0] : DRAX_NIL_VAL;
}

drax_value __d_scalar_tail(d_vm* vm, int* stat) {
  #define REMOVE_VAL_TO_TYPE_TAIL(_tp, _l1, _l2)\
    _tp* _dv1 = (_tp*) _l1->elems;\
    _tp* _dv2 = (_tp*) _l2->elems;\
    memcpy(_dv2, &_dv1[1], _l2->length * sizeof(_tp));

  drax_value a = pop(vm);

  return_if_is_not_scalar(a, stat);
  drax_scalar* l1 = CAST_SCALAR(a);

  drax_scalar* l = new_dscalar(vm, l1->length -1, l1->_stype);
  l->length = l1->length - 1;

  if (DIT_f32 == l->_stype) {
    REMOVE_VAL_TO_TYPE_TAIL(float, l1, l);
  } else if (DIT_f64 == l->_stype) {
    REMOVE_VAL_TO_TYPE_TAIL(double, l1, l);
  } else {
    l->elems = l1->elems + 1;
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(l);
}

drax_value __d_scalar_length(d_vm* vm, int* stat) {
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  drax_scalar* l = CAST_SCALAR(a);

  DX_SUCESS_FN(stat);
  return AS_VALUE(l->length);
}

drax_value __d_scalar_is_empty(d_vm* vm, int* stat) {
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  drax_scalar* l = CAST_SCALAR(a);

  DX_SUCESS_FN(stat);
  return l->length ? DRAX_FALSE_VAL : DRAX_TRUE_VAL;
}

drax_value __d_scalar_is_present(d_vm* vm, int* stat) {
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  drax_scalar* l = CAST_SCALAR(a);

  DX_SUCESS_FN(stat);
  return l->length ? DRAX_TRUE_VAL : DRAX_FALSE_VAL;
}

drax_value __d_scalar_remove_at(d_vm* vm, int* stat) {
  #define REMOVE_VAL_TO_TYPE_R_AT(_tp, _l1, _l2)\
    _tp* _dv1 = (_tp*) _l1->elems;\
    _tp* _dv2 = (_tp*) _l2->elems;\
    memcpy(_dv2, _dv1, at * sizeof(_tp));\
    memcpy(_dv2 + at, _dv1 + at + 1, (_l2->length - at) * sizeof(_tp));

  drax_value b = pop(vm);
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  return_if_is_not_number(b, stat);

  drax_scalar* l = CAST_SCALAR(a);
  int at = (int) CAST_NUMBER(b);
  at = at < 0 ? l->length + at : at;

  if(at >= l->length) {
    DX_SUCESS_FN(stat);
    return DS_VAL(new_dscalar(vm, 0, DIT_UNDEFINED));
  }

  drax_scalar* nl = new_dscalar(vm, l->length - 1, l->_stype);
  nl->length = l->length - 1;

  if (DIT_f32 == l->_stype) {
    REMOVE_VAL_TO_TYPE_R_AT(float, l, nl);
  } else if (DIT_f64 == l->_stype) {
    REMOVE_VAL_TO_TYPE_R_AT(double, l, nl);
  } else {
    memcpy(nl->elems, l->elems, at * sizeof(drax_value));
    memcpy(nl->elems + at, l->elems + at + 1, (nl->length - at) * sizeof(drax_value));
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(nl);
}

drax_value __d_scalar_insert_at(d_vm* vm, int* stat) {
  #define INSET_AT_TO_TYPE(_tp, _l1, _l2)\
    _tp* _dv1 = (_tp*) _l1->elems;\
    _tp* _dv2 = (_tp*) _l2->elems;\
    _tp nc = (_tp) CAST_NUMBER(c);\
    memcpy(_dv2, _dv1, at * sizeof(_tp));\
    memcpy(&_dv2[at], &nc, sizeof(_tp));\
    memcpy(_dv2 + at + 1, _dv1 + at, (_l1->length - at) * sizeof(_tp));

  drax_value c = pop(vm);
  drax_value b = pop(vm);
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  return_if_is_not_number(b, stat);

  drax_scalar* l = CAST_SCALAR(a);
  int at = (int) CAST_NUMBER(b);
  at = at < 0 ? l->length + at : at;

  if(at >= l->length) {
    DX_SUCESS_FN(stat);
    return DS_VAL(new_dscalar(vm, 0, DIT_UNDEFINED));
  }

  drax_scalar* nl = new_dscalar(vm, l->length + 1, l->_stype);
  nl->length = l->length + 1;

  if (DIT_f32 == l->_stype) {
    INSET_AT_TO_TYPE(float, l, nl);
  } else if (DIT_f64 == l->_stype) {
    INSET_AT_TO_TYPE(double, l, nl);
  } else {
    memcpy(nl->elems, l->elems, at * sizeof(drax_value));
    memcpy(&nl->elems[at], &c, sizeof(drax_value));
    memcpy(nl->elems + at + 1, l->elems + at, (l->length - at) * sizeof(drax_value));
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(nl);
}

drax_value __d_scalar_replace_at(d_vm* vm, int* stat) {
  #define REPLACE_AT_TO_TYPE(_tp, _l1, _l2)\
    _tp* _dv1 = (_tp*) _l1->elems;\
    _tp* _dv2 = (_tp*) _l2->elems;\
    _tp nc = (_tp) CAST_NUMBER(c);\
    memcpy(_dv2, _dv1, at * sizeof(_tp));\
    memcpy(&_dv2[at], &nc, sizeof(_tp));\
    memcpy(_dv2 + at + 1, _dv1 + at + 1, (_l1->length - at - 1) * sizeof(_tp));

  drax_value c = pop(vm);
  drax_value b = pop(vm);
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  return_if_is_not_number(b, stat);

  drax_scalar* l = CAST_SCALAR(a);
  int at = (int) CAST_NUMBER(b);
  at = at < 0 ? l->length + at : at;

  if(at >= l->length) {
    DX_SUCESS_FN(stat);
    return DS_VAL(new_dscalar(vm, 0, DIT_UNDEFINED));
  }
  
  drax_scalar* nl = new_dscalar(vm, l->length, l->_stype);
  nl->length = l->length;

  if (DIT_f32 == l->_stype) {
    REPLACE_AT_TO_TYPE(float, l, nl);
  } else if (DIT_f64 == l->_stype) {
    REPLACE_AT_TO_TYPE(double, l, nl);
  } else {
    memcpy(nl->elems, l->elems, at * sizeof(drax_value));
    memcpy(&nl->elems[at], &c, sizeof(drax_value));
    memcpy(nl->elems + at + 1, l->elems + at + 1, (l->length  - at - 1) * sizeof(drax_value));
  }
  
  DX_SUCESS_FN(stat);
  return DS_VAL(nl);
}

drax_value __d_scalar_slice(d_vm* vm, int* stat) {
  #define S_SLICE_TO_TYPE(_tp, _l1, _l2)\
    _tp* _dv1 = (_tp*) _l1->elems;\
    _tp* _dv2 = (_tp*) _l2->elems;\
    memcpy(_dv2, _dv1 + from, abs(to - from) * sizeof(_tp));

  drax_value c = pop(vm);
  drax_value b = pop(vm);
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  return_if_is_not_number(b, stat);

  drax_scalar* l = CAST_SCALAR(a);
  int from = (int) CAST_NUMBER(b);
  int to = (int) CAST_NUMBER(c);

  from = from < 0 ? l->length + from : from;
  to = to < 0 ? l->length + to : to;

  if(to <= from || from >= l->length || to > l->length) {
    DX_SUCESS_FN(stat);
    return DS_VAL(new_dscalar(vm, 0, DIT_UNDEFINED));
  }

  drax_scalar* nl = new_dscalar(vm, abs(to - from), l->_stype);
  nl->length = abs(to - from);

  if (DIT_f32 == l->_stype) {
    S_SLICE_TO_TYPE(float, l, nl);
  } else if (DIT_f64 == l->_stype) {
    S_SLICE_TO_TYPE(double, l, nl);
  } else {
    memcpy(nl->elems, l->elems + from, abs(to - from) * sizeof(drax_value));
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(nl);
}

drax_value __d_scalar_sum(d_vm* vm, int* stat) {
  drax_value a = pop(vm);
  return_if_is_not_scalar(a, stat);
  
  drax_scalar* l = CAST_SCALAR(a);

  if(l->length == 0) {
    DX_SUCESS_FN(stat);
    return AS_VALUE(0);
  }

  if (l->_stype != DIT_f32 && l->_stype != DIT_f64) {
    DX_ERROR_FN(stat);
    return DS_VAL(new_derror(vm, (char *) "Expected scalar of number as argument"));
  }
  
  double res = 0;
  int i;

  if (DIT_f32 == l->_stype) {
    float f32res = 0;
    float* _f32 = (float*) l->elems;
    for (i = 0; i < l->length; i++) f32res += _f32[i];
    res = (double) f32res;
  }

  if (DIT_f64 == l->_stype) {
    double* _f64 = (double*) l->elems;
    for (i = 0; i < l->length; i++) res += _f64[i];
  }

  DX_SUCESS_FN(stat);
  return AS_VALUE(res);
}

drax_value __d_scalar_sparse(d_vm* vm, int* stat) {
  drax_value a = pop(vm);
  return_if_is_not_number(a, stat);

  int n = (int) CAST_NUMBER(a);

  if(n < 0) {
    DX_SUCESS_FN(stat);
    return DS_VAL(new_dscalar(vm, 0, DIT_f64));
  }

  drax_scalar* ll = new_dscalar(vm, n, DIT_f64);
  ll->length = n;
  double v = 0;
  double* _v = (double*) ll->elems;

  int i;
  for(i = 0; i < ll->length; i++) {
    _v[i] = v;
  }

  DX_SUCESS_FN(stat);
  return DS_VAL(ll);
}