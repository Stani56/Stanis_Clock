# 🔄 Tier 1 Integration - Rollback Instructions

## Quick Rollback to Stable Version

If any issues occur with the Tier 1 integration, use these commands to immediately revert to the stable version:

### **Emergency Rollback (Immediate)**
```bash
# Return to stable main branch
git checkout main

# Force clean any uncommitted changes
git reset --hard HEAD

# Verify you're on stable version
git log --oneline -5
```

### **Alternative: Keep Integration Branch for Future**
```bash
# Switch to main but keep integration branch
git checkout main

# Optional: Create backup of integration work
git tag tier1-integration-backup tier1-integration
```

## Branch Structure

- **`main`**: ✅ **STABLE** - Production-ready wordclock system
- **`tier1-integration`**: 🔧 **EXPERIMENTAL** - Integration work in progress

## Integration Phases and Checkpoints

### Phase 1: Component Integration ✅
- [x] Add Tier 1 components to build system
- [x] Basic compilation and initialization testing
- **Rollback Point**: If compilation fails

### Phase 2: MQTT Manager Integration (IN PROGRESS)
- [ ] Integrate schema validator with existing MQTT manager
- [ ] Add command processor to MQTT command handling
- [ ] Replace direct MQTT calls with persistent delivery
- **Rollback Point**: If MQTT functionality breaks

### Phase 3: Real Command Integration 
- [ ] Register real wordclock schemas
- [ ] Add real command handlers
- [ ] Test with Home Assistant
- **Rollback Point**: If commands stop working

### Phase 4: System Testing
- [ ] Hardware testing on ESP32
- [ ] Performance and reliability testing  
- [ ] Integration with all existing features
- **Rollback Point**: If any existing feature breaks

## Safety Checks

Before each phase, verify:
```bash
# Check build succeeds
idf.py build

# Check system starts normally  
idf.py flash monitor

# Check MQTT connectivity
# Check Home Assistant discovery
# Check basic commands work
```

## Emergency Contacts

- **Current Working System**: Commit `a94fc5c` on `main` branch
- **Last Known Good**: All existing wordclock features working
- **Test Commands**: `restart`, `status`, basic MQTT Discovery

## Recovery Procedure

1. **Immediate**: `git checkout main && git reset --hard HEAD`
2. **Clean Build**: `idf.py fullclean && idf.py build`
3. **Flash**: `idf.py flash`
4. **Verify**: Check all existing functionality works
5. **Report**: Document any issues for future integration attempts

---

**Remember: The main branch should ALWAYS remain stable and functional!** 🛡️