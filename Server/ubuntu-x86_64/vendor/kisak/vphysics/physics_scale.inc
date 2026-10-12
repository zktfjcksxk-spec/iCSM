// Uniform positive scale, following mdlcache.cpp's flScale > 0 contract.
// IVP compact surfaces store relative offsets, shared point arrays, unit-mass
// inertia, and normalized box extents. Scale geometry before rebuilding caches.
static void ScaleCompactTree( IVP_Compact_Ledgetree_Node *node, float scale,
                              std::set<IVP_Compact_Poly_Point *> &points )
{
    for ( int axis = 0; axis < 3; ++axis ) node->center.k[axis] *= scale;
    node->radius *= scale;
    const IVP_Compact_Ledge *ledge = node->get_compact_hull();
    if ( ledge )
    {
        const IVP_Compact_Triangle *triangle = ledge->get_first_triangle();
        for ( int i = 0; i < ledge->get_n_triangles(); ++i, ++triangle )
            for ( int j = 0; j < 3; ++j )
            {
                IVP_Compact_Poly_Point *point = const_cast<IVP_Compact_Poly_Point *>(
                    triangle->get_edge(j)->get_start_point(ledge) );
                if ( points.insert(point).second )
                    for ( int axis = 0; axis < 3; ++axis ) point->k[axis] *= scale;
            }
    }
    if ( !node->is_terminal() )
    {
        ScaleCompactTree( const_cast<IVP_Compact_Ledgetree_Node *>(node->left_son()), scale, points );
        ScaleCompactTree( const_cast<IVP_Compact_Ledgetree_Node *>(node->right_son()), scale, points );
    }
}

void CPhysicsCollision::DuplicateAndScale( vcollide_t *out, const vcollide_t *in, float scale )
{
    if ( !out || !in || out == in || !(scale > 0) || !IsFinite(scale) )
        Error( "DuplicateAndScale requires distinct vcollides and finite positive scale\n" );
    memset( out, 0, sizeof(*out) );
    out->solidCount = in->solidCount;
    out->descSize = in->descSize;
    out->solids = new CPhysCollide *[in->solidCount]();
    for ( int i = 0; i < in->solidCount; ++i )
    {
        if ( !in->solids[i] ) continue;
        const IVP_Compact_Surface *original = in->solids[i]->GetCompactSurface();
        if ( !original ) Error( "DuplicateAndScale: non-compact collision model\n" );
        IVP_Compact_Surface *surface = static_cast<IVP_Compact_Surface *>(
            ivp_malloc_aligned(original->get_size(), 16) );
        memcpy( surface, original, original->get_size() );
        for ( int axis = 0; axis < 3; ++axis )
        {
            surface->mass_center.k[axis] *= scale;
            surface->rotation_inertia.k[axis] *= scale * scale;
        }
        surface->upper_limit_radius *= scale;
        std::set<IVP_Compact_Poly_Point *> points;
        ScaleCompactTree( const_cast<IVP_Compact_Ledgetree_Node *>(
            surface->get_compact_ledge_tree_root()), scale, points );
        CPhysCollideCompactSurface *copy = new CPhysCollideCompactSurface(surface);
        surface->dummy[0] = in->solids[i]->GetVCollideIndex();
        copy->SetOrthographicAreas( in->solids[i]->GetOrthographicAreas() );
        out->solids[i] = copy;
    }
    if ( in->pKeyValues )
    {
        size_t size = strlen(in->pKeyValues) + 1;
        out->pKeyValues = new char[size];
        memcpy( out->pKeyValues, in->pKeyValues, size );
    }
    // Opaque cache user data has no size/clone contract; callers rebuild it.
}
