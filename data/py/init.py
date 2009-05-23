import pykpai
import threading
import sys

from pykpai import GAME_SPEED, MAX_UNITS, SQUARE_SIZE

config = {
}




default_config = {
        # if attacking group has an important target in this radius,
        # it'll be less likely that it retreats
        'importantRadius': 1000.0,

        # control of units that don't exit base (e.g. builders on spooler map)
        'maxBaseStuckCount': 3,
        'baseSearchRadius': 16.0,
        
        'retreatGroupTimeout': 15*GAME_SPEED,
        'attackStateChangeTimeout': 90*GAME_SPEED,

        'gatherMinOffset': 8.0*SQUARE_SIZE,
        'gatherMaxOffset': 24.0*SQUARE_SIZE,

        'baseDefenseRadius': 1536.0,

        # when the gather group exceeds this number of units,
        # go straight to enemy base
        'rushBaseUnitCount': 250,

        # the following values determine when and where to retreat builders
        'builderRetreatMaxDist': 40.0*SQUARE_SIZE,
        'builderRetreatMinDist': 10.0*SQUARE_SIZE,
        'builderRetreatCheckOffset': 10.0*SQUARE_SIZE,

        # units
        'spam_radius': 384.0,

        'system_spam': 'bit',
        'system_heavy': 'byte',
        'system_arty': 'pointer',
        'pointer_radius': 1400.0,

        'hacker_spam': 'bug',
        'hacker_heavy': 'worm',
        'hacker_arty': 'dos',
        'dos_radius': 768.0,

        'network_spam': 'packet',
        'network_heavy': 'connection',
        'network_arty': 'flow',
        'flow_radius': 500.0,

        # probabilities
        'pr_MOVEOnAttack': 0.1,
}

# put default values into the configuration
for k in default_config:
    if not k in config:
        config[k] = default_config[k]

def get_config_value(name):
    return config.get(name, None)

def get_wanted_constructors(geospots, width, height):
    return max(geospots//4, 1)

def get_build_spot_priority(distance, influence, width, height):
    return int(influence - distance/((width+height)*15))

def game_frame(teamId, frame):
    import pykpai
    print 'frame:',frame
    #pykpai.SendTextMessage(teamId, "OH HAI in frame %s"%frame)


def dump_status(teamId, frame, geos, friends, foes):
    print frame
    print list(geos)
    print len(geos)
    for f3 in geos:
        print f3
    print friends
    print foes

