//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#pragma once

#include "kv3lib/keyvalues3.h"
#include "schemalib/schemaclassinfo.h"

void NormalizeKV3TreeToSchemaClasses( KeyValues3 *pRoot );
void RemoveKeysInKV3TreeEqualToDefault( KeyValues3 *pRoot );

void ImprintSchemaClassOnKV3( KeyValues3 *pTarget, const char *pClassName );
void RemoveKeysNotInClass( KeyValues3 *pTarget, const char *pClassName );
void RemoveKeysInTableEqualToDefault_Shallow( KeyValues3 *pTarget, const char *pClassName );
void RemoveKeysInTableEqualToDefault_Shallow( KeyValues3 *pTarget, ClassIntrospectionHandle_t pClass );
void ImprintSchemaTypeOnKV3( KeyValues3 *pData, FieldIntrospectionHandle_t pField, const CSchemaType *pType, const char *pValue );
void ImprintSchemaClassOnKV3( KeyValues3 *pTarget, ClassIntrospectionHandle_t pClass );
bool KV3IsEqualToDefault( KeyValues3 *pTarget, FieldIntrospectionHandle_t pAssociatedField, const char *pDefaultValue );
