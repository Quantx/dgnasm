struct mccsubtype subtype_val = {
    0, 0, NULL, NULL, NULL
};

struct mccsubtype subtype_ptr = {
    1, 0, NULL, NULL, NULL
};

struct mcctype type_char = {
    CPL_CHR,
    NULL,
    &subtype_val
};

struct mcctype type_int = {
    CPL_INT,
    NULL,
    &subtype_val
};

struct mcctype type_long = {
    CPL_LNG,
    NULL,
    &subtype_val
};

struct mcctype type_string = {
    CPL_CHR,
    NULL,
    &subtype_ptr
};

// Size required to store type in bytes
int16_t typeSize( struct mcctype * t )
{
    // Get active subtype
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    if ( s->ftype ) mccfail("cannot get sizeof function");

    // Check if this is an array
    int16_t i, j;
    for ( i = 0, j = 0; i < s->arrays; i++ )
    {
        if ( !s->sizes[i] ) mccfail("cannot get sizeof unknown dimension in array");
        j *= s->sizes[i];
    }
    if ( i ) return j;

    // Check if this is a pointer
    if ( s->inder ) return 2;

    // Check if this is a struct
    if ( t->stype )
    {
        if ( ~t->stype->type & CPL_DEFN ) mccfail("tried to get sizeof incomplete type");
        return t->stype->size;
    }

    // Must be a primative type
    int8_t ptype = t->ptype & CPL_DTYPE_MASK;

    // Can't get size of void
    if ( ptype == CPL_VOID ) mccfail("tried to get sizeof void");

    if ( ptype <= CPL_UCHR ) return 1;
    if ( ptype <= CPL_UINT ) return 2;
    if ( ptype <= CPL_FPV  ) return 4;

    return 8; // Double
}

struct mcctype * typeClone( struct mcctype * t )
{
    struct mcctype * out = sbrk(sizeof(struct mcctype));
    if ( out == SBRKFAIL ) mccfail("unable to allocate new type");

    out->ptype = t->ptype;
    out->stype = t->stype;

    struct mccsubtype * s, ** os = &out->sub;
    for ( s = t->sub; s; s = s->sub )
    {
        *os = sbrk(sizeof(struct mccsubtype));
        if ( *os == SBRKFAIL ) mccfail("unable to allocate new subtype");

        (*os)->inder = s->inder;
        (*os)->arrays = s->arrays;

        (*os)->sizes = sbrk(s->arrays);
        if ( (*os)->sizes == SBRKFAIL ) mccfail("unable to allocate new size list");

        int16_t i;
        for ( i = 0; i < s->arrays; i++ ) (*os)->sizes[i] = s->sizes[i];

        (*os)->ftype = s->ftype; // This should be safe since function namespaces are also immutable

        os = &(*os)->sub;
    }

    return out;
}

// Inderect to type
struct mcctype * typeInder( struct mcctype * t )
{
    struct mcctype * out = typeClone(t);

    // Get active subtype
    struct mccsubtype * s;
    for ( s = out->sub; s->sub; s = s->sub );

    if ( s->arrays || s->ftype )
    {
        s = s->sub = sbrk(sizeof(struct mccsubtype));
        if ( s == SBRKFAIL ) mccfail("unable to allocate new subtype");

        s->inder = 1;
        s->arrays = 0;
        s->sizes = NULL;
        s->ftype = NULL;
        s->sub = NULL;
    }
    else s->inder++;

    return out;
}

// Dereference type
struct mcctype * typeDeref( struct mcctype * t )
{
    struct mcctype * out = typeClone(t);

    struct mccsubtype * s, * ls = NULL;
    for ( s = out->sub; s->sub; s = s->sub ) ls = s;

    if ( s->ftype ) mccfail("tried to dereference a function");
    else if ( s->arrays ) s->arrays--; // TODO do we drop the first or last size index here?
    else if ( s->inder ) s->inder--;
    else mccfail("tried to dereference non-pointer");

    // Reduce type if needed
    if ( ls && !s->inder && !s->arrays && !s->ftype ) ls->sub = NULL;

    return out;
}

// Prote arithmetic types
struct mcctype * typePromote( struct mcctype * ta, struct mcctype * tb )
{
    unsigned int8_t pa = ta->ptype & CPL_DTYPE_MASK;
    unsigned int8_t pb = tb->ptype & CPL_DTYPE_MASK;

    if ( pa >= pb ) return ta;
    else return tb;
}

/*
    CPL Type hierarchy:

    scalar > arith(metic) > integer

    integer = char, int, long
    arith(metic) = integer, float
    scalar = arith(metic), pointer
*/

int16_t isInteger( struct mcctype * t )
{
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    unsigned int8_t p = t->ptype & CPL_DTYPE_MASK;

    return !(s->ftype || s->inder || s->arrays || t->stype ) && p && p <= CPL_ULNG;
}

int16_t isArith( struct mcctype * t )
{
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    return !(s->ftype || s->inder || s->arrays || t->stype ) && t->ptype & CPL_DTYPE_MASK;
}

int16_t isPointer( struct mcctype * t )
{
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    return !s->ftype && (s->inder || s->arrays);
}

int16_t isScalar( struct mcctype * t )
{
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    return !s->ftype && ( s->inder || s->arrays || !t->stype ) && t->ptype & CPL_DTYPE_MASK;
}

int16_t isFunction( struct mcctype * t )
{
    // Get active subtype
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    return s->ftype == NULL;
}

int16_t isStruct( struct mcctype * t )
{
    return t->stype && !isFunction(t);
}

int16_t isArray( struct mcctype * t )
{
    struct mccsubtype * s;
    for ( s = t->sub; s->sub; s = s->sub );

    return s->arrays;
}

int16_t isCompatible( struct mcctype * ta, struct mcctype * tb )
{
    // If either is a structure, make sure they're compatible
    if ( ta->stype != tb->stype ) return 0;

/* All primative types are compatible
    // Ensure primative types match
    if ( !ta->stype && (ta->ptype & CPL_DTYPE_MASK) != (tb->ptype & CPL_DTYPE_MASK) ) return 0;
*/
    // Get active subtype
    struct mccsubtype * sa, * sb;
    for ( sa = ta->sub; sa->sub; sa = sa->sub );
    for ( sb = tb->sub; sb->sub; sb = sb->sub );

    // Make sure array dimensions match
    if ( sa->arrays != sb->arrays ) return 0;

    if ( sa->ftype )
    {
        if ( !sb->ftype ) return 0;

        // TODO check function compatibility
    }

    return 1;
}