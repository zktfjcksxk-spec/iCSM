// Diagnostic switch retains the original lookup for same-process comparisons.
static ConVar icsm_attribute_lookup_fast("icsm_attribute_lookup_fast", "1", FCVAR_RELEASE,
    "Index-first item attribute lookup and exact per-iteration negative filter");
static bool ICSMFastAttributeLookup() { return icsm_attribute_lookup_fast.GetBool(); }
