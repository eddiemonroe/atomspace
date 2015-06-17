from opencog.atomspace cimport cHandle, tv_ptr, cAtomSpace

cdef extern from "opencog/cython/opencog/BindlinkStub.h" namespace "opencog":
    # C++: 
    #   Handle stub_bindlink(AtomSpace*, Handle);
    #
    cdef cHandle c_stub_bindlink "stub_bindlink" (cAtomSpace*, cHandle)
    cdef cHandle c_execute_atom "do_execute"(cAtomSpace*, cHandle)


cdef extern from "opencog/query/BindLinkAPI.h" namespace "opencog":
    # C++: 
    #   Handle bindlink(AtomSpace*, Handle);
    #   Handle single_bindlink (AtomSpace*, Handle);
    #   Handle af_bindlink(AtomSpace*, Handle);
    #   TruthValuePtr satisfaction_link(AtomSpace*, Handle);
    #
    cdef cHandle c_bindlink "bindlink" (cAtomSpace*, cHandle)
    cdef cHandle c_single_bindlink "single_bindlink" (cAtomSpace*, cHandle)
    cdef cHandle c_af_bindlink "af_bindlink" (cAtomSpace*, cHandle)
    cdef tv_ptr c_satisfaction_link "satisfaction_link" (cAtomSpace*, cHandle)


cdef extern from "opencog/atoms/execution/EvaluationLink.h" namespace "opencog":
    tv_ptr c_evaluate_atom "opencog::EvaluationLink::do_evaluate"(cAtomSpace*, cHandle)
