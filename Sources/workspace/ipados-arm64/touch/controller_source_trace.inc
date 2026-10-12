// Bounded diagnostics only; never reads KeyState (which consumes key edges).
static void TraceControllerInput(int stage, int slot, const CUserCmd *cmd)
{
    static double next[8]={};
    if (stage<0 || stage>=8 || !ICSMControllerTraceActive()) return;
    const double now=Plat_FloatTime();
    if (now<next[stage]) return;
    next[stage]=now+0.1;
    QAngle viewangles;engine->GetViewAngles(viewangles);
    kbutton_t *keys[]={&in_forward,&in_back,&in_moveleft,&in_moveright,
        &in_left,&in_right,&in_lookup,&in_lookdown};
    char keyText[512]={};
    size_t used=0;
    for (int i=0;i<ARRAYSIZE(keys);++i)
    {
        const auto &k=keys[i]->GetPerUser(slot);
        const int n=V_snprintf(keyText+used,sizeof(keyText)-used,"%d:%d/%d/%d%s",
            i,k.state,k.down[0],k.down[1],i+1==ARRAYSIZE(keys)?"":",");
        if (n<0 || size_t(n)>=sizeof(keyText)-used) break;
        used+=size_t(n);
    }
    Msg("ICSM_CONTROLLER_STAGE t=%.6f frame=%d stage=%d cmd=%d move=%g,%g view=%g,%g touch=%g,%g,%u keys=%s\n",
        now,gpGlobals->framecount,stage,cmd?cmd->command_number:-1,
        cmd?cmd->forwardmove:0,cmd?cmd->sidemove:0,viewangles[YAW],viewangles[PITCH],
        s_moveX,s_moveY,s_lookCount,keyText);
}
