set(TEST_SOURCES
    __init__.py
    test_basecclass.py
    test_basecenum.py
    test_basecvalue.py
    test_cwrap.py
)

add_python_package("python.tests.core.cwrap" ${PYTHON_INSTALL_PREFIX}/tests/core/cwrap "${TEST_SOURCES}" False)

addPythonTest(core.cwrap tests.core.cwrap.test_cwrap.CWrapTest)
addPythonTest(core.cwrap.basecclass tests.core.cwrap.test_basecclass.BaseCClassTest)
addPythonTest(core.cwrap.basecenum tests.core.cwrap.test_basecenum.BaseCEnumTest)
addPythonTest(core.cwrap.basecvalue tests.core.cwrap.test_basecvalue.BaseCValueTest)

