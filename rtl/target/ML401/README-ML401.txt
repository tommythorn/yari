YARI does build on ML401, but due ISE with it's binary project format,
I'm unable to successfully create a complete project here.

However, making one from scratch isn't too hard:
- Create a new project
- Use an Virtex 4, XC4XL25, speed -10
- Add these sources:

        config.h
        dpram.v
        dpram_simple.v
        rs232.v
        simpledpram.v
        top.v
        ml401.ucf

        ../../soclib/rs232in.v
        ../../soclib/rs232out.v

        ../../yari-core/asm.v
        ../../yari-core/perfcounters.v
        ../../yari-core/stage_D.v
        ../../yari-core/stage_I.v
        ../../yari-core/stage_M.v
        ../../yari-core/stage_X.v
        ../../yari-core/yari.v

Voila !

