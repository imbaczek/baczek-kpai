import pykpai
import threading
import sys

config = {
}




default_config = {
        'importantRadius': 1000,
        # units
        'system_spam': 'bit',
        'system_heavy': 'byte',
        'system_arty': 'pointer',
        'pointer_radius': 1400,

        'hacker_spam': 'bug',
        'hacker_heavy': 'worm',
        'hacker_arty': 'dos',
        'dos_radius': 768,

        'network_spam': 'packet',
        'network_heavy': 'connection',
        'network_arty': 'flow',
        'flow_radius': 1000,
}

# put default values into the configuration
for k in default_config:
    if not k in config:
        config[k] = default_config[k]

def get_config_value(name):
    return config.get(name, None)

def get_wanted_constructors(geospots, width, height):
    return min(geospots//4, 3)


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

