// The same topology invariants as IVP_Compact_Ledge_Solver::check_ledge(),
// available in Release too, with compact-surface range checks before dereference.
static_assert( sizeof(IVP_Compact_Surface) == 48, "serialized IVPS surface layout" );
static_assert( sizeof(IVP_Compact_Ledge) == 16, "serialized IVPS ledge layout" );
static_assert( sizeof(IVP_Compact_Triangle) == 16, "serialized IVPS triangle layout" );
static_assert( sizeof(IVP_Compact_Poly_Point) == 16, "serialized IVPS point layout" );
static void CheckCompactVCollide( vcollide_t *vcollide, const char *name )
{
    if ( !vcollide || (vcollide->solidCount && !vcollide->solids) )
        Error( "VCollideCheck: missing solids in %s\n", name );
    for ( int i = 0; i < vcollide->solidCount; ++i )
    {
        if ( !vcollide->solids[i] ) continue;
        const IVP_Compact_Surface *surface = vcollide->solids[i]->GetCompactSurface();
        if ( !surface ) Error( "VCollideCheck: non-compact solid in %s\n", name );
        const uintptr_t begin = reinterpret_cast<uintptr_t>(surface);
        const uintptr_t end = begin + surface->get_size();
        auto inside = [begin,end]( const void *ptr, size_t bytes ) {
            uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
            return start >= begin && start <= end && bytes <= end - start;
        };
        if ( (begin & 15) || surface->get_size() < sizeof(*surface) ||
             !inside(surface->get_compact_ledge_tree_root(),sizeof(IVP_Compact_Ledgetree_Node)) )
            Error( "VCollideCheck: invalid compact surface in %s\n", name );
        IVP_U_BigVector<IVP_Compact_Ledge> ledges;
        vcollide->solids[i]->GetAllLedges(ledges);
        for ( int j = 0; j < ledges.len(); ++j )
        {
            const IVP_Compact_Ledge *ledge = ledges.element_at(j);
            if ( !inside(ledge,sizeof(*ledge)) || (reinterpret_cast<uintptr_t>(ledge)&15) )
                Error( "VCollideCheck: invalid ledge in %s\n", name );
            const IVP_Compact_Triangle *first = ledge->get_first_triangle();
            const int count = ledge->get_n_triangles();
            if ( count <= 0 || !inside(first,count*sizeof(*first)) )
                Error( "VCollideCheck: invalid triangle range in %s\n", name );
            for ( int triangle = 0; triangle < count; ++triangle )
            {
                const IVP_Compact_Triangle *tri = first + triangle;
                if ( tri->get_tri_index() != triangle )
                    Error( "VCollideCheck: invalid triangle index in %s\n", name );
                for ( int k = 0; k < 3; ++k )
                {
                    const IVP_Compact_Edge *edge = tri->get_edge(k);
                    const IVP_Compact_Edge *opposite = edge->get_opposite();
                    uintptr_t oppositeAddress = reinterpret_cast<uintptr_t>(opposite);
                    uintptr_t trianglesBegin = reinterpret_cast<uintptr_t>(first);
                    uintptr_t trianglesEnd = trianglesBegin + count*sizeof(*first);
                    if ( oppositeAddress < trianglesBegin || oppositeAddress+sizeof(*edge) > trianglesEnd ||
                         (oppositeAddress&15) < 4 || (oppositeAddress&3) )
                        Error( "VCollideCheck: invalid opposite edge in %s\n", name );
                    if ( opposite->get_opposite() != edge ||
                         edge->get_start_point_index() != opposite->get_next()->get_start_point_index() )
                        Error( "VCollideCheck: broken edge topology in %s\n", name );
                    const IVP_Compact_Poly_Point *point = edge->get_start_point(ledge);
                    if ( !inside(point,sizeof(*point)) || (reinterpret_cast<uintptr_t>(point)&15) ||
                         !IsFinite(point->k[0]) || !IsFinite(point->k[1]) || !IsFinite(point->k[2]) )
                        Error( "VCollideCheck: invalid compact vertex in %s\n", name );
                }
            }
        }
    }
}
