#!/bin/bash
diff \
<(./bin/make_world  10 0.1 | ./bin/step_world 0.1 1000) \
<(./bin/make_world 10 0.1 | ./$1 0.1 1000 )

