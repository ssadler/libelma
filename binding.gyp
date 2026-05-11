{
  'targets': [
    {
      'target_name': 'elma',
      'sources': [ 'binding.cc' ],
      "libraries": [
        "../elma/build/libelma.so"
      ],
      #"library_dirs": ["../elma/build"],

      #"link_settings": {
      #  "libraries": [
      #    "-L../elma/build/elma.so"
      #  ],
      #},

      'dependencies': [
        "<!(node -p \"require('node-addon-api').targets\"):node_addon_api",
      ],
    }
  ]
}
