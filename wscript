# vim: ft=python sts=4 ts=8 et sw=4

import os.path

srcdir = '.'
blddir = 'output'
copydir = os.path.join(srcdir, 'CopiedSpringFiles')

def set_options(opt):
    opt.add_option('--spring-dir', default='../../../',
            help='Spring RTS checkout directory')
    opt.add_option('--boost-dir',
            help='directory from which include boost/*.h works')


def configure(conf):
    # tool checks
    conf.check_tool('gcc')
    conf.check_tool('g++')
    conf.find_gxx()
    # global compiler flags
    conf.env.append_value('CCFLAGS',  '-Wall')
    conf.env.append_value('CXXFLAGS',  '-Wall')
    import Options
    if Options.platform=='win32':
        conf.env.append_value('shlib_LINKFLAGS', ['-Wl,--kill-at'])
        conf.env['shlib_PATTERN'] = '%s.dll'
    # global conf options
    from Options import options
    conf.env['spring_dir'] = options.spring_dir
    conf.env['boost_dir'] = options.boost_dir

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
    import glob, os.path, Options
    # waf 1.5.3 cannot compile files outside of srcdir, need to copy needed
    # stuff
    def copy_spring_files():
        print 'copying spring files...'
        import shutil
        springdir = bld.env['spring_dir']
        tocopy = \
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.cpp')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'CUtils', '*.c')) +\
            glob.glob(os.path.join(springdir, 'AI', 'Wrappers', 'LegacyCPP', '*.cpp')) +\
            [os.path.join(springdir, 'rts', 'System', 'float3.cpp'),
                os.path.join(springdir, 'rts', 'Game', 'GameVersion.cpp'),
                os.path.join(springdir, 'rts', 'Sim', 'Misc', 'DamageArray.cpp')]
        if not os.path.isdir(copydir):
            os.makedirs(copydir)
        for f in tocopy:
            shutil.copy2(f, copydir)
    def clean_spring_files():
        print 'cleaning spring files...'
        import shutil
        if os.path.isdir(copydir):
            shutil.rmtree(copydir)
        
    if Options.commands['clean']:
        clean_spring_files()
    else:
        copy_spring_files()

    skirmishai = bld.new_task_gen('cxx', 'shlib')
    skirmishai.defines = 'BUILDING_SKIRMISH_AI BUILDING_AI'
    skirmishai.source = \
            glob.glob(os.path.join(srcdir, '*.cpp')) +\
            glob.glob(os.path.join(srcdir, 'GUI', '*.cpp')) +\
            glob.glob(os.path.join(copydir, '*.cpp')) +\
            glob.glob(os.path.join(copydir, '*.c'))
    skirmishai.includes = ['.'] + [os.path.join(bld.env['spring_dir'], x)
                for x in ('rts', 'rts/System', 'AI/Wrappers',
                    'AI/Wrappers/CUtils', 'AI/Wrappers/LegacyCPP',
                    'rts/Sim/Misc', 'rts/Game')] \
            + [bld.env['boost_dir']]
    skirmishai.target = 'SkirmishAI'
