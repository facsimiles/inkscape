// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Wraps up the Wasmer API to something easier to use with C++
*/

namespace wasmer {

class error : std::exception {
    std::string _error;

  public:
    error(const std::string &explainer = "Wasmer error")
    {
        auto errarray = std::vector<char>(wasmer_last_error_length());
        wasmer_last_error_message(errarray.data(), errarray.size());
        _error = explainer + ": " + std::string{ errarray.data(), errarray.size() };
    }
    virtual const char *what() const noexcept override { return _error.c_str(); }
    static void check(wasmer_result_t result, const std::string &text)
    {
        if (result != wasmer_result_t::WASMER_OK) {
            throw error{ text };
        }
    }
};

template <typename T>
class value {
    T val;

  public:
    value(T in)
        : val(in)
    {
    }
    value(wasmer_value_t &vt)
    {
        if (vt.tag == wasmer_value_tag::WASM_I32) {
            val = vt.value.I32;
        } else if (vt.tag == wasmer_value_tag::WASM_I64) {
            val = vt.value.I64;
        } else if (vt.tag == wasmer_value_tag::WASM_F32) {
            val = vt.value.F32;
        } else if (vt.tag == wasmer_value_tag::WASM_F64) {
            val = vt.value.F64;
        }
    }
    operator wasmer_value_t() const { return to_value_t(val); }
    operator T() const { return val; }

  private:
    static wasmer_value_t to_value_t(const int32_t &i)
    {
        return wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = i } };
    }
    static wasmer_value_t to_value_t(const int64_t &i)
    {
        return wasmer_value_t{ .tag = wasmer_value_tag::WASM_I64, .value = wasmer_value{ .I64 = i } };
    }
    static wasmer_value_t to_value_t(const float &i)
    {
        return wasmer_value_t{ .tag = wasmer_value_tag::WASM_F32, .value = wasmer_value{ .F32 = i } };
    }
    static wasmer_value_t to_value_t(const double &i)
    {
        return wasmer_value_t{ .tag = wasmer_value_tag::WASM_F64, .value = wasmer_value{ .F64 = i } };
    }
};

class import {
  public:
    import() {}
    operator wasmer_import_t() const { return wasmer_import_t{}; }
};

class memory {
    const wasmer_memory_t *_mem;
    wasmer_memory_t *_dmem;

  public:
    memory(wasmer_memory_t *mem, bool destroy = true)
        : _mem(mem)
        , _dmem(mem)
    {
    }
    memory(const wasmer_memory_t *mem, bool destroy = false)
        : _mem(mem)
        , _dmem(nullptr)
    {
    }
    ~memory()
    {
        if (_dmem) {
            wasmer_memory_destroy(_dmem);
        }
    }

    size_t size() { return wasmer_memory_data_length((wasmer_memory_t *)_mem); }

    uint8_t *ptr(int32_t addr)
    {
        if (addr >= size()) {
            throw std::runtime_error("Accessing memory outside the size of the memory");
        }
        auto base = wasmer_memory_data(_mem);
        base += addr;
        return base;
    }

    int32_t data(int32_t addr)
    {
        auto p = ptr(addr);
        auto p32 = reinterpret_cast<int32_t *>(p);
        return *p32;
    }
};

class instance {
    wasmer_instance_t *_instance;
    std::string _malloc_name;
    std::string _free_name;

    // TODO: I imagine these will require searching exports, but not quite sure
    // how they'll show up for other types of programs
    void find_malloc() { _malloc_name = "__wbindgen_malloc"; }

    void find_free() { _free_name = "__wbindgen_free"; }

  public:
    instance(std::string &module, std::vector<import> imports = {})
    {
        std::vector<wasmer_import_t> imp{ imports.begin(), imports.end() };
        auto result =
            wasmer_instantiate(&_instance, (uint8_t *)(module.c_str()), module.size(), imp.data(), imp.size());
        wasmer::error::check(result, "Wasmer unable to create instance");
    }
    ~instance()
    {
        if (_instance) {
            wasmer_instance_destroy(_instance);
        }
    }
    operator wasmer_instance_t *() const { return _instance; }
    int32_t malloc(int32_t size)
    {
        if (_malloc_name.empty()) {
            find_malloc();
        }

        auto [addr] = call<std::tuple<int32_t>>(_malloc_name, size);
        return addr;
    }

    void free(int32_t addr, int32_t size)
    {
        if (_free_name.empty()) {
            find_free();
        }

        call<std::tuple<>>(_free_name, addr, size);
    }

    class heapHandle {
        int32_t addr;
        int32_t size;
        instance *inst;

      public:
        heapHandle(instance *ininst, int32_t inaddr, int32_t insize)
            : inst(ininst)
            , addr(inaddr)
            , size(insize)
        {
        }
        ~heapHandle() { inst->free(addr, size); }
    };

    std::tuple<int32_t, heapHandle> heapAllocate(int32_t size)
    {
        auto addr = malloc(size);
        return std::make_tuple(addr, heapHandle{ this, addr, size });
    }

    template <typename rettuple, typename... inputvals>
    rettuple call(const std::string &funcname, inputvals... params)
    {
        std::array<wasmer_value_t, sizeof...(params)> vparams{ wasmer::value<inputvals>(params)... };
        std::array<wasmer_value_t, std::tuple_size<rettuple>::value> vret;

        printf("Calling '%s'\n", funcname.c_str());
        auto result =
            wasmer_instance_call(_instance, funcname.c_str(), vparams.data(), vparams.size(), vret.data(), vret.size());
        wasmer::error::check(result, "Wasmer instance execution error");

        rettuple ret;
        call_return(ret, vret);

        return ret;
    }

  private:
    template <size_t cnt = 0, typename tupletype, typename arraytype>
        inline typename std::enable_if <
        cnt<std::tuple_size<tupletype>::value, void>::type call_return(tupletype tuple, arraytype array)
    {
        using tupleelement = typename std::tuple_element<cnt, tupletype>::type;
        std::get<cnt>(tuple) = (tupleelement)value<tupleelement>(array[cnt]);
        call_return<(cnt + 1), tupletype, arraytype>(tuple, array);
        return;
    }

    template <size_t cnt = 0, typename tupletype, typename arraytype>
    inline typename std::enable_if<cnt == std::tuple_size<tupletype>::value, void>::type call_return(tupletype tuple,
                                                                                                     arraytype array)
    {
    }

  public:
    std::shared_ptr<memory> mem()
    {
        auto ctx = wasmer_instance_context_get(_instance);

        return std::make_shared<memory>(wasmer_instance_context_memory(ctx, 0));
    }
};

} // namespace wasmer
