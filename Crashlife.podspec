Pod::Spec.new do |s|
  s.name                  = "Crashlife"
  s.version               = "1.0.2"
  s.summary               = "Awesome crash reporting 😎"
  s.description           = "Get crash reports from your iOS app!"
  s.homepage              = "https://www.buglife.com"
  s.license               = { "type" => "Apache", :file => 'LICENSE' }
  s.author                = { "Buglife" => "support@buglife.com" }
  s.source                = { "git" => "https://github.com/Buglife/Crashlife-iOS.git", :tag => s.version.to_s }
  s.platform              = :ios, '9.0'
  s.source_files          = "Source/**/*.{c,cpp,m,h,def}"
  s.public_header_files   = "Source/*.{h}"
end
