# Tracing

The tracing component is a small project-wide instrumentation boundary. It
provides no-op scope macros in normal builds and connects them to Tracy when
`OBZ_MARKET_LAB_ENABLE_TRACY` is enabled.

Components depend on the `obz_market_lab::tracing` interface target explicitly,
so tracing does not need to be owned by an unrelated domain component or placed
in a general-purpose `common` directory.
