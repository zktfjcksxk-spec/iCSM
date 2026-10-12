
#include <tier0/platform.h>

#if VALVE_CPP11

#include <tier1/utlpointers.h>
#include <tier1/utlvector.h>

namespace {
	struct PEx {
		PEx( int v = 0 ) : mValue( v ) { DevMsg( "0x%p def_cons %x\n", this, mValue ); }
		~PEx() { DevMsg( "0x%p destruct %x\n", this, mValue ); }
		PEx( const PEx& other ) : mValue( other.mValue ) { DevMsg( "0x%p copycons 0x%p %x\n", this, &other, mValue ); }
		PEx( PEx&& other ) : mValue( other.mValue ) { DevMsg( "0x%p movecons 0x%p %x\n", this, &other, mValue ); other.mValue = 0xbadf00d; }
		PEx& operator=( const PEx& other ) { DevMsg( "0x%p copyassn 0%p %x -> %x\n", this, &other, mValue, other.mValue ); mValue = other.mValue; return *this; }
		PEx& operator=( PEx&& other ) { DevMsg( "0x%p moveassn 0x%p %x -> %x\n", this, &other, mValue, other.mValue ); mValue = other.mValue; other.mValue = 0xbadf00d; return *this; }

		int mValue;
	};

	struct PExDer : public PEx {
		PExDer() : PEx( 5 ), mValue2( 5 ) { DevMsg( "0x%p derived cons\n", this ); }
		~PExDer() { DevMsg( "0x%p derived destruct\n", this ); }

		int mValue2;
	};

	template <typename C>
	void TPs()
	{
		UtlRefCountedPtr<PEx, C> ptr = UtlRefCountedPtr<PEx, C>( PTR_CONSTRUCT );
		*ptr = 1;
		Assert( ptr );
		UtlRefCountedPtr<PEx, C> ptr2 = ptr;
		Assert( ptr2 );
		UtlRefCountedPtr<PEx, C> ptr3 = Move( ptr2 );
		Assert( ptr3 );
		Assert( !ptr2 );
		UtlWeakPtr<PEx, C> ptr4 = ptr;
		UtlWeakPtr<PEx, C> ptr5 = ptr;
		ptr5 = ptr4;
		ptr2 = ptr5;
		Assert( ptr2 );
		ptr = ptr2 = ptr3 = nullptr;
		ptr2 = ptr5;
		Assert( !ptr2 ); // ptr5 was weak and should have been free'd since no strong refs remain
		Assert( !ptr4 ); // weak reference should be dead
		ptr4 = nullptr;
		ptr5 = nullptr;

		ptr = UtlRefCountedPtr<PExDer, C>( PTR_CONSTRUCT );
		ptr = nullptr;

	}
} // anonymous namespace

void TestUtlPtrs()
{
	TPs<int>();
	TPs<CInterlockedIntT<int>>();

	{
		CUtlVector< UtlOwnedPtr< PEx > > vecExample;

		PEx* tmp = new PEx( 4 );

		vecExample.AddToTail( UtlOwnedPtr<PEx>( PTR_CONSTRUCT, 2 ) );
		vecExample.AddToTail( UtlOwnedPtr<PEx>( PTR_CONSTRUCT, 3 ) );
		vecExample.AddToTail( UtlOwnedPtr<PEx>( PTR_GIVE_OWNERSHIP, tmp ) );
		vecExample.AddToTail( UtlOwnedPtr<PEx>( PTR_CONSTRUCT, 5 ) );
		vecExample.AddToTail( UtlOwnedPtr<PEx>( PTR_CONSTRUCT, 6 ) );

		DevMsg( "%d\n", vecExample[3]->mValue );
	}

	{
		UtlOwnedPtr<PExDer> pOwned( PTR_CONSTRUCT );
		UtlRefCountedPtr<PEx> pShared( Move( pOwned ) );
		Assert( !pOwned );
		Assert( pShared );
	}
}

#endif
