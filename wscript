# vim: ft=python sts=4 ts=8 et sw=4

import os.path

VERSION='0.0.1'
APPNAME='BaczekKPAI'
srcdir = '.'
blddir = 'output'


def set_options(opt):
    opt.add_option('--spring-dir', default='../../../',
            help='Spring RTS checkout directory')
    opt.add_option('--boost-dir',
            help='directory from which include boost/*.h works')


def configure(conf):
    # tool checks
    conf.check_tool('gxx')
    #conf.find_gxx()
    # global compiler flags
    conf.env.append_value('CCFLAGS',  '-Wall')
    conf.env.append_value('CXXFLAGS',  '-Wall')
    import Options
    if Options.platform=='win32':
        conf.env.append_value('shlib_LINKFLAGS', ['-Wl,--kill-at'])
        conf.env['shlib_PATTERN'] = '%s.dll'
    # global conf options
    from Options import options
    conf.env['spring_dir'] = os.path.abspath(options.spring_dir)
    conf.env['boost_dir'] = os.path.abspath(options.boost_dir)

    # more compiler flags
    env2 = conf.env.copy()
    conf.env.append_value('CCFLAGS', '-O2')
    conf.env.append_value('CXXFLAGS', '-O2')
    conf.set_env_name('debug', env2)
    # debug flags
    conf.setenv('debug')
    conf.env.append_value('CCFLAGS', '-g')
    conf.env.append_value('CXXFLAGS', '-g')
    

def build(bld):
    import glob, os.path
    def get_spring_files():
        springdir = bld.env['spring_dir']
        tocopy = \
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.cpp')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.c')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'LegacyCPP', '*.cpp')) +\
            [os.path.join(springdir, 'rts', 'System', 'float3.cpp'),
                os.path.join(springdir, 'rts', 'Game', 'GameVersion.cpp'),
                os.path.join(springdir, 'rts', 'Sim', 'Misc', 'DamageArray.cpp')]
        return tocopy

    spring_files = get_spring_files()
    for f in spring_files:
        bld.new_task_gen(
                name='copy',
                before='cxx',
                target=os.path.split(f)[-1],
                rule='cp -p %s ${TGT}'%f,
                always=True,
                on_results=True,
        )
    skirmishai = bld.new_task_gen(
            features='cxx cshlib',
            includes=['.']+ [os.path.join(bld.env['spring_dir'], x)
                for x in ('rts', 'rts/System', 'AI/Wrappers',
                    'AI/Wrappers/CUtils', 'AI/Wrappers/LegacyCPP',
                    'rts/Sim/Misc', 'rts/Game')] \
                + [bld.env['boost_dir']],
            source= \
                glob.glob(os.path.join(srcdir, '*.cpp')) +\
                glob.glob(os.path.join(srcdir, 'GUI', '*.cpp')) +\
                glob.glob(os.path.join(srcdir, 'json_spirit', '*.cpp')) +\
                [os.path.split(f)[-1]
                        for f in spring_files],
            defines='BUILDING_SKIRMISH_AI BUILDING_AI',
            target='SkirmishAI',
    )
