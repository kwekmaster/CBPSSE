#pragma once

namespace CBP
{
    class DRender :
        ILog
    {
        typedef void (*presentCallback_t)();
        typedef void (*createD3D11_t)(void);
        typedef void (*unkPresent)(uint32_t p1);

    public:
        static bool Initialize();

        inline static void AddPresentCallback(presentCallback_t f) {
            m_Instance.m_presentCallbacks.push_back(f);
        }

        FN_NAMEPROC("Render")
    private:
        DRender() = default;

        static inline auto CreateD3D11 = IAL::Addr(75595, 0x9);
        static inline auto UnkPresent = IAL::Addr(75461, 0x9);

        createD3D11_t CreateD3D11_O;
        unkPresent UnkPresent_O;

        static void Present_Pre(uint32_t);
        static void CreateD3D11_Hook();

        std::vector<presentCallback_t> m_presentCallbacks;

        static DRender m_Instance;
    };

    class D3D11CreateEvent
    {
    public:
        D3D11CreateEvent(
            CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc) :
            m_pSwapChainDesc(pSwapChainDesc)
        {}

        CONST DXGI_SWAP_CHAIN_DESC* const m_pSwapChainDesc;
    };

    class D3D11CreateEventPost :
        public D3D11CreateEvent
    {
    public:
        D3D11CreateEventPost(
            CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
            ID3D11Device* pDevice,
            ID3D11DeviceContext* pImmediateContext,
            IDXGISwapChain* pSwapChain
        ) :
            D3D11CreateEvent(pSwapChainDesc),
            m_pDevice(pDevice),
            m_pImmediateContext(pImmediateContext),
            m_pSwapChain(pSwapChain)
        {}

        ID3D11Device* const m_pDevice;
        ID3D11DeviceContext* const m_pImmediateContext;
        IDXGISwapChain* const m_pSwapChain;
    };
}