import esphome.codegen as cg

storage_base_component_ns = cg.esphome_ns.namespace("storage_base")
StorageBase = storage_base_component_ns.class_("StorageBase", cg.Component)
